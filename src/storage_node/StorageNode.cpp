#include "StorageNode.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>

namespace fs = std::filesystem;

namespace dfs {

StorageNode::StorageNode(const std::string& node_id, const std::string& ip, int port, 
                         const std::string& mds_ip, int mds_port, const std::string& storage_dir)
    : m_node_id(node_id), m_ip(ip), m_port(port), m_mds_ip(mds_ip), m_mds_port(mds_port), 
      m_storage_dir(storage_dir), m_running(false) {
          
    if (!fs::exists(m_storage_dir)) {
        fs::create_directories(m_storage_dir);
    }
}

StorageNode::~StorageNode() {
    Stop();
}

void StorageNode::Start() {
    if (!m_server_sock.Bind(m_port) || !m_server_sock.Listen()) {
        std::cerr << "Failed to start StorageNode on port " << m_port << "\n";
        return;
    }

    m_running = true;
    m_accept_thread = std::thread(&StorageNode::AcceptLoop, this);
    m_heartbeat_thread = std::thread(&StorageNode::HeartbeatLoop, this);
    
    std::cout << "Storage Node " << m_node_id << " started on port " << m_port << "\n";
}

void StorageNode::Stop() {
    m_running = false;
    m_server_sock.Close();
    if (m_accept_thread.joinable()) m_accept_thread.join();
    if (m_heartbeat_thread.joinable()) m_heartbeat_thread.join();
    
    std::lock_guard<std::mutex> lock(m_threads_mutex);
    for (auto& t : m_client_threads) {
        if (t.joinable()) t.join();
    }
}

void StorageNode::AcceptLoop() {
    while (m_running) {
        auto client_sock = m_server_sock.Accept();
        if (client_sock) {
            std::lock_guard<std::mutex> lock(m_threads_mutex);
            m_client_threads.emplace_back(&StorageNode::HandleClient, this, std::move(client_sock));
        }
    }
}

void StorageNode::HeartbeatLoop() {
    while (m_running) {
        TCPSocket sock;
        if (sock.Connect(m_mds_ip, m_mds_port)) {
            json req = {
                {"type", "HEARTBEAT"},
                {"node_id", m_node_id},
                {"ip", m_ip},
                {"port", m_port},
                {"capacity", GetCapacity()},
                {"used_space", GetUsedSpace()}
            };
            
            SendMessage(sock, req);
            json res;
            RecvMessage(sock, res);
        } else {
            std::cerr << "Failed to send heartbeat to MDS.\n";
        }
        
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
}

void StorageNode::HandleClient(std::unique_ptr<TCPSocket> client_sock) {
    json req;
    if (RecvMessage(*client_sock, req)) {
        std::string type = req.value("type", "");
        if (type == "UPLOAD_CHUNK") {
            HandleUploadChunk(*client_sock, req);
        } else if (type == "DOWNLOAD_CHUNK") {
            HandleDownloadChunk(*client_sock, req);
        } else if (type == "REPLICATE") {
            HandleReplicateChunk(*client_sock, req);
        } else {
            json res = {{"status", "ERROR"}, {"message", "Unknown request type"}};
            SendMessage(*client_sock, res);
        }
    }
}

void StorageNode::HandleUploadChunk(TCPSocket& sock, const json& req) {
    std::string chunk_id = req["chunk_id"];
    
    std::vector<char> data;
    if (!RecvChunkData(sock, data)) {
        json res = {{"status", "ERROR"}, {"message", "Failed to receive chunk data"}};
        SendMessage(sock, res);
        return;
    }

    std::string filepath = m_storage_dir + "/" + chunk_id;
    std::ofstream out(filepath, std::ios::binary);
    if (out.is_open()) {
        out.write(data.data(), data.size());
        out.close();
        json res = {{"status", "OK"}};
        SendMessage(sock, res);
    } else {
        json res = {{"status", "ERROR"}, {"message", "Failed to write chunk to disk"}};
        SendMessage(sock, res);
    }
}

void StorageNode::HandleDownloadChunk(TCPSocket& sock, const json& req) {
    std::string chunk_id = req["chunk_id"];
    std::string filepath = m_storage_dir + "/" + chunk_id;
    
    std::ifstream in(filepath, std::ios::binary | std::ios::ate);
    if (in.is_open()) {
        std::streamsize size = in.tellg();
        in.seekg(0, std::ios::beg);
        
        std::vector<char> data(size);
        if (in.read(data.data(), size)) {
            json res = {{"status", "OK"}, {"size", size}};
            SendMessage(sock, res);
            SendChunkData(sock, data);
            return;
        }
    }
    
    json res = {{"status", "ERROR"}, {"message", "Chunk not found or read error"}};
    SendMessage(sock, res);
}

void StorageNode::HandleReplicateChunk(TCPSocket& sock, const json& req) {
    std::string chunk_id = req["chunk_id"];
    std::string target_ip = req["target_ip"];
    int target_port = req["target_port"];

    std::string filepath = m_storage_dir + "/" + chunk_id;
    std::ifstream in(filepath, std::ios::binary | std::ios::ate);
    if (!in.is_open()) {
        json res = {{"status", "ERROR"}, {"message", "Chunk not found locally"}};
        SendMessage(sock, res);
        return;
    }

    std::streamsize size = in.tellg();
    in.seekg(0, std::ios::beg);
    std::vector<char> data(size);
    in.read(data.data(), size);

    TCPSocket target_sock;
    if (target_sock.Connect(target_ip, target_port)) {
        json fwd_req = {
            {"type", "UPLOAD_CHUNK"},
            {"chunk_id", chunk_id}
        };
        SendMessage(target_sock, fwd_req);
        SendChunkData(target_sock, data);
        
        json fwd_res;
        if (RecvMessage(target_sock, fwd_res) && fwd_res["status"] == "OK") {
            json res = {{"status", "OK"}};
            SendMessage(sock, res);
            return;
        }
    }

    json res = {{"status", "ERROR"}, {"message", "Failed to replicate to target"}};
    SendMessage(sock, res);
}

int64_t StorageNode::GetCapacity() {
    // Return mock capacity or actual filesystem capacity
    // For simplicity, let's say 10GB
    return 10LL * 1024 * 1024 * 1024;
}

int64_t StorageNode::GetUsedSpace() {
    int64_t size = 0;
    if (fs::exists(m_storage_dir)) {
        for (const auto& entry : fs::directory_iterator(m_storage_dir)) {
            if (fs::is_regular_file(entry)) {
                size += fs::file_size(entry);
            }
        }
    }
    return size;
}

} // namespace dfs
