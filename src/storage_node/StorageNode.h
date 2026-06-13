#pragma once

#include "Message.h"
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <mutex>

namespace dfs {

class StorageNode {
public:
    StorageNode(const std::string& node_id, const std::string& ip, int port, 
                const std::string& mds_ip, int mds_port, const std::string& storage_dir);
    ~StorageNode();

    void Start();
    void Stop();

private:
    void AcceptLoop();
    void HandleClient(std::unique_ptr<TCPSocket> client_sock);
    void HeartbeatLoop();
    
    // Request Handlers
    void HandleUploadChunk(TCPSocket& sock, const json& req);
    void HandleDownloadChunk(TCPSocket& sock, const json& req);
    void HandleReplicateChunk(TCPSocket& sock, const json& req);

    int64_t GetUsedSpace();
    int64_t GetCapacity();

    std::string m_node_id;
    std::string m_ip;
    int m_port;
    
    std::string m_mds_ip;
    int m_mds_port;
    
    std::string m_storage_dir;

    TCPSocket m_server_sock;
    std::atomic<bool> m_running;
    
    std::thread m_accept_thread;
    std::thread m_heartbeat_thread;
    std::vector<std::thread> m_client_threads;
    std::mutex m_threads_mutex;
};

} // namespace dfs
