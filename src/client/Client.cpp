#include "Client.h"
#include "Message.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <thread>
#include <future>

namespace fs = std::filesystem;

namespace dfs {

Client::Client(const std::string& mds_ip, int mds_port) : m_mds_ip(mds_ip), m_mds_port(mds_port) {}

bool Client::UploadFile(const std::string& local_filepath, const std::string& remote_filename) {
    if (!fs::exists(local_filepath)) {
        std::cerr << "Local file does not exist.\n";
        return false;
    }

    int64_t size = fs::file_size(local_filepath);
    int num_chunks = (size + CHUNK_SIZE - 1) / CHUNK_SIZE;

    TCPSocket mds_sock;
    if (!mds_sock.Connect(m_mds_ip, m_mds_port)) {
        std::cerr << "Failed to connect to MDS.\n";
        return false;
    }

    json req = {
        {"type", "UPLOAD_REQ"},
        {"filename", remote_filename},
        {"size", size},
        {"num_chunks", num_chunks}
    };
    SendMessage(mds_sock, req);

    json res;
    if (!RecvMessage(mds_sock, res)) {
        std::cerr << "Failed to receive response from MDS.\n";
        return false;
    }

    if (res["status"] != "OK") {
        std::cerr << "Upload rejected by MDS: " << res.value("message", "") << "\n";
        return false;
    }

    std::ifstream in(local_filepath, std::ios::binary);
    if (!in.is_open()) return false;

    auto chunks = res["chunks"];
    std::vector<std::future<bool>> futures;

    for (int i = 0; i < num_chunks; ++i) {
        std::string chunk_id = chunks[i]["chunk_id"];
        auto nodes = chunks[i]["nodes"];
        
        size_t bytes_to_read = std::min(CHUNK_SIZE, static_cast<size_t>(size - i * CHUNK_SIZE));
        std::vector<char> data(bytes_to_read);
        in.read(data.data(), bytes_to_read);

        // Upload in parallel
        futures.push_back(std::async(std::launch::async, [chunk_id, nodes, data]() {
            bool success = false;
            // Try uploading to nodes (in a real system, maybe upload to one and let it replicate, 
            // or upload to all 3 in parallel). Here we upload to all 3.
            for (const auto& node : nodes) {
                TCPSocket sock;
                if (sock.Connect(node["ip"], node["port"])) {
                    json req = {
                        {"type", "UPLOAD_CHUNK"},
                        {"chunk_id", chunk_id}
                    };
                    SendMessage(sock, req);
                    SendChunkData(sock, data);
                    
                    json res;
                    if (RecvMessage(sock, res) && res["status"] == "OK") {
                        success = true; // At least one successful upload
                    }
                }
            }
            return success;
        }));
    }

    bool all_success = true;
    for (auto& f : futures) {
        if (!f.get()) {
            all_success = false;
        }
    }

    if (all_success) {
        std::cout << "File uploaded successfully.\n";
    } else {
        std::cerr << "Some chunks failed to upload.\n";
    }

    return all_success;
}

bool Client::DownloadFile(const std::string& remote_filename, const std::string& local_filepath) {
    TCPSocket mds_sock;
    if (!mds_sock.Connect(m_mds_ip, m_mds_port)) {
        std::cerr << "Failed to connect to MDS.\n";
        return false;
    }

    json req = {
        {"type", "DOWNLOAD_REQ"},
        {"filename", remote_filename}
    };
    SendMessage(mds_sock, req);

    json res;
    if (!RecvMessage(mds_sock, res)) {
        std::cerr << "Failed to receive response from MDS.\n";
        return false;
    }

    if (res["status"] != "OK") {
        std::cerr << "Download rejected by MDS: " << res.value("message", "") << "\n";
        return false;
    }

    auto chunks = res["chunks"];
    std::vector<std::vector<char>> downloaded_data(chunks.size());
    std::vector<std::future<bool>> futures;

    for (size_t i = 0; i < chunks.size(); ++i) {
        std::string chunk_id = chunks[i]["chunk_id"];
        auto nodes = chunks[i]["nodes"];
        
        futures.push_back(std::async(std::launch::async, [i, chunk_id, nodes, &downloaded_data]() {
            for (const auto& node : nodes) {
                TCPSocket sock;
                if (sock.Connect(node["ip"], node["port"])) {
                    json req = {
                        {"type", "DOWNLOAD_CHUNK"},
                        {"chunk_id", chunk_id}
                    };
                    SendMessage(sock, req);
                    
                    json res;
                    if (RecvMessage(sock, res) && res["status"] == "OK") {
                        std::vector<char> data;
                        if (RecvChunkData(sock, data)) {
                            downloaded_data[i] = std::move(data);
                            return true; // Successfully downloaded
                        }
                    }
                }
            }
            return false; // Failed to download from all replicas
        }));
    }

    bool all_success = true;
    for (auto& f : futures) {
        if (!f.get()) {
            all_success = false;
        }
    }

    if (!all_success) {
        std::cerr << "Failed to download some chunks.\n";
        return false;
    }

    std::ofstream out(local_filepath, std::ios::binary);
    if (!out.is_open()) return false;

    for (const auto& data : downloaded_data) {
        out.write(data.data(), data.size());
    }

    std::cout << "File downloaded successfully.\n";
    return true;
}

} // namespace dfs
