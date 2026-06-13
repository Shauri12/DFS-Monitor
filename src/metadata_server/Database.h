#pragma once

#include <string>
#include <vector>
#include <mutex>
#include <sqlite3.h>

namespace dfs {

struct NodeInfo {
    std::string node_id;
    std::string ip;
    int port;
    int64_t last_heartbeat;
    int64_t capacity;
    int64_t used_space;
};

struct ChunkLocation {
    std::string chunk_id;
    std::string node_id;
};

class Database {
public:
    Database(const std::string& db_path);
    ~Database();

    bool Init();

    // Node Management
    bool UpsertNode(const std::string& node_id, const std::string& ip, int port, int64_t capacity, int64_t used_space);
    std::vector<NodeInfo> GetActiveNodes(int64_t current_time, int64_t timeout_sec);
    std::vector<NodeInfo> GetDeadNodes(int64_t current_time, int64_t timeout_sec);
    void RemoveNode(const std::string& node_id);

    // File Management
    bool AddFile(const std::string& filename, int64_t size);
    bool FileExists(const std::string& filename);

    // Chunk Management
    bool AddChunk(const std::string& chunk_id, const std::string& filename, int chunk_index);
    bool AddChunkLocation(const std::string& chunk_id, const std::string& node_id);
    std::vector<std::string> GetChunkLocations(const std::string& chunk_id);
    std::vector<std::string> GetChunksForFile(const std::string& filename);
    std::vector<std::string> GetChunksOnNode(const std::string& node_id);
    void RemoveChunkLocationsOnNode(const std::string& node_id);

private:
    bool Execute(const std::string& sql);
    
    sqlite3* m_db;
    std::mutex m_mutex;
};

} // namespace dfs
