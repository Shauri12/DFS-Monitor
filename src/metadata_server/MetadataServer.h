#pragma once

#include "Database.h"
#include "Message.h"
#include <memory>
#include <thread>
#include <atomic>
#include <vector>

namespace dfs {

class MetadataServer {
public:
    MetadataServer(int port, const std::string& db_path);
    ~MetadataServer();

    void Start();
    void Stop();

private:
    void AcceptLoop();
    void HandleClient(std::unique_ptr<TCPSocket> client_sock);
    
    // Message Handlers
    void HandleHeartbeat(TCPSocket& sock, const json& req);
    void HandleUploadRequest(TCPSocket& sock, const json& req);
    void HandleDownloadRequest(TCPSocket& sock, const json& req);
    
    // Background Tasks
    void MonitorNodes();
    void SelfHealNode(const std::string& node_id);

    int m_port;
    Database m_db;
    TCPSocket m_server_sock;
    std::atomic<bool> m_running;
    
    std::thread m_accept_thread;
    std::thread m_monitor_thread;
    std::vector<std::thread> m_client_threads;
    std::mutex m_threads_mutex;
    
    const int HEARTBEAT_TIMEOUT_SEC = 15;
    const int REPLICATION_FACTOR = 3;
};

} // namespace dfs
