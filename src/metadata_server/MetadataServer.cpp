#include "MetadataServer.h"
#include <iostream>
#include <chrono>
#include <algorithm>


namespace dfs {

MetadataServer::MetadataServer(int port, const std::string& db_path)
    : m_port(port), m_db(db_path), m_running(false) {
}

MetadataServer::~MetadataServer() {
    Stop();
}

void MetadataServer::Start() {
    if (!m_db.Init()) {
        std::cerr << "Failed to init Database.\n";
        return;
    }

    if (!m_server_sock.Bind(m_port) || !m_server_sock.Listen()) {
        std::cerr << "Failed to start MetadataServer on port " << m_port << "\n";
        return;
    }

    m_running = true;
    m_accept_thread = std::thread(&MetadataServer::AcceptLoop, this);
    m_monitor_thread = std::thread(&MetadataServer::MonitorNodes, this);
    
    std::cout << "Metadata Server started on port " << m_port << "\n";
}

void MetadataServer::Stop() {
    m_running = false;
    m_server_sock.Close();
    if (m_accept_thread.joinable()) m_accept_thread.join();
    if (m_monitor_thread.joinable()) m_monitor_thread.join();
    
    std::lock_guard<std::mutex> lock(m_threads_mutex);
    for (auto& t : m_client_threads) {
        if (t.joinable()) t.join();
    }
}

void MetadataServer::AcceptLoop() {
    while (m_running) {
        auto client_sock = m_server_sock.Accept();
        if (client_sock) {
            std::lock_guard<std::mutex> lock(m_threads_mutex);
            m_client_threads.emplace_back(&MetadataServer::HandleClient, this, std::move(client_sock));
        }
    }
}

void MetadataServer::HandleClient(std::unique_ptr<TCPSocket> client_sock) {
    json req;
    if (RecvMessage(*client_sock, req)) {
        std::string type = req.value("type", "");
        if (type == "HEARTBEAT") {
            HandleHeartbeat(*client_sock, req);
        } else if (type == "UPLOAD_REQ") {
            HandleUploadRequest(*client_sock, req);
        } else if (type == "DOWNLOAD_REQ") {
            HandleDownloadRequest(*client_sock, req);
        } else {
            json res = {{"status", "ERROR"}, {"message", "Unknown request type"}};
            SendMessage(*client_sock, res);
        }
    }
}

void MetadataServer::HandleHeartbeat(TCPSocket& sock, const json& req) {
    std::string node_id = req["node_id"];
    std::string ip = req["ip"];
    int port = req["port"];
    int64_t capacity = req["capacity"];
    int64_t used_space = req["used_space"];

    if (m_db.UpsertNode(node_id, ip, port, capacity, used_space)) {
        json res = {{"status", "OK"}};
        SendMessage(sock, res);
    } else {
        json res = {{"status", "ERROR"}};
        SendMessage(sock, res);
    }
}

void MetadataServer::HandleUploadRequest(TCPSocket& sock, const json& req) {
    std::string filename = req["filename"];
    int64_t size = req["size"];
    int num_chunks = req["num_chunks"];

    if (m_db.FileExists(filename)) {
        json res = {{"status", "ERROR"}, {"message", "File already exists"}};
        SendMessage(sock, res);
        return;
    }

    auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto active_nodes = m_db.GetActiveNodes(now, HEARTBEAT_TIMEOUT_SEC);

    if (active_nodes.size() < REPLICATION_FACTOR) {
        json res = {{"status", "ERROR"}, {"message", "Not enough active nodes for replication"}};
        SendMessage(sock, res);
        return;
    }

    // Sort nodes by load (used_space / capacity)
    std::sort(active_nodes.begin(), active_nodes.end(), [](const NodeInfo& a, const NodeInfo& b) {
        double loadA = a.capacity > 0 ? (double)a.used_space / a.capacity : 1.0;
        double loadB = b.capacity > 0 ? (double)b.used_space / b.capacity : 1.0;
        return loadA < loadB;
    });

    m_db.AddFile(filename, size);
    
    json res;
    res["status"] = "OK";
    res["chunks"] = json::array();

    for (int i = 0; i < num_chunks; ++i) {
        std::string chunk_id = filename + "_part" + std::to_string(i);
        m_db.AddChunk(chunk_id, filename, i);
        
        json chunk_info;
        chunk_info["chunk_id"] = chunk_id;
        chunk_info["nodes"] = json::array();
        
        // Select least loaded nodes (simple round robin or top N)
        // For production, we should update simulated loads here, but for simplicity we just pick top 3
        for (int j = 0; j < REPLICATION_FACTOR; ++j) {
            int node_idx = (i + j) % active_nodes.size(); // basic distribution
            const auto& node = active_nodes[node_idx];
            m_db.AddChunkLocation(chunk_id, node.node_id);
            
            json n;
            n["node_id"] = node.node_id;
            n["ip"] = node.ip;
            n["port"] = node.port;
            chunk_info["nodes"].push_back(n);
        }
        res["chunks"].push_back(chunk_info);
    }

    SendMessage(sock, res);
}

void MetadataServer::HandleDownloadRequest(TCPSocket& sock, const json& req) {
    std::string filename = req["filename"];
    
    if (!m_db.FileExists(filename)) {
        json res = {{"status", "ERROR"}, {"message", "File not found"}};
        SendMessage(sock, res);
        return;
    }

    auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto active_nodes = m_db.GetActiveNodes(now, HEARTBEAT_TIMEOUT_SEC);
    
    // Map of node_id to NodeInfo for quick lookup
    std::map<std::string, NodeInfo> active_map;
    for (const auto& n : active_nodes) {
        active_map[n.node_id] = n;
    }

    json res;
    res["status"] = "OK";
    res["chunks"] = json::array();
    
    auto chunk_ids = m_db.GetChunksForFile(filename);
    for (const auto& cid : chunk_ids) {
        json chunk_info;
        chunk_info["chunk_id"] = cid;
        chunk_info["nodes"] = json::array();
        
        auto locs = m_db.GetChunkLocations(cid);
        for (const auto& loc : locs) {
            if (active_map.count(loc)) {
                json n;
                n["node_id"] = loc;
                n["ip"] = active_map[loc].ip;
                n["port"] = active_map[loc].port;
                chunk_info["nodes"].push_back(n);
            }
        }
        res["chunks"].push_back(chunk_info);
    }
    
    SendMessage(sock, res);
}

void MetadataServer::MonitorNodes() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        
        auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
        auto dead_nodes = m_db.GetDeadNodes(now, HEARTBEAT_TIMEOUT_SEC);
        
        for (const auto& node : dead_nodes) {
            std::cout << "Node " << node.node_id << " is DEAD. Initiating Self-Healing.\n";
            SelfHealNode(node.node_id);
            m_db.RemoveNode(node.node_id);
            m_db.RemoveChunkLocationsOnNode(node.node_id);
        }
    }
}

void MetadataServer::SelfHealNode(const std::string& node_id) {
    // Find all chunks that were on this node
    auto chunks = m_db.GetChunksOnNode(node_id);
    if (chunks.empty()) return;

    auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    auto active_nodes = m_db.GetActiveNodes(now, HEARTBEAT_TIMEOUT_SEC);

    for (const auto& cid : chunks) {
        // Find existing healthy replicas
        auto locs = m_db.GetChunkLocations(cid);
        std::string source_node_id;
        NodeInfo source_node;
        bool found_source = false;

        for (const auto& loc : locs) {
            if (loc != node_id) {
                auto it = std::find_if(active_nodes.begin(), active_nodes.end(), [&](const NodeInfo& n) {
                    return n.node_id == loc;
                });
                if (it != active_nodes.end()) {
                    source_node = *it;
                    found_source = true;
                    break;
                }
            }
        }

        if (!found_source) {
            std::cerr << "Warning: Data loss for chunk " << cid << ". No healthy replicas found.\n";
            continue;
        }

        // Find a new target node that doesn't already have this chunk
        NodeInfo target_node;
        bool found_target = false;
        for (const auto& n : active_nodes) {
            if (std::find(locs.begin(), locs.end(), n.node_id) == locs.end()) {
                target_node = n;
                found_target = true;
                break;
            }
        }

        if (found_target) {
            // Update metadata
            m_db.AddChunkLocation(cid, target_node.node_id);
            
            // Command the source node to copy the chunk to the target node
            json req = {
                {"type", "REPLICATE"},
                {"chunk_id", cid},
                {"target_ip", target_node.ip},
                {"target_port", target_node.port}
            };

            TCPSocket sock;
            if (sock.Connect(source_node.ip, source_node.port)) {
                SendMessage(sock, req);
                std::cout << "Replicating chunk " << cid << " from " << source_node.node_id << " to " << target_node.node_id << "\n";
            }
        }
    }
}

} // namespace dfs
