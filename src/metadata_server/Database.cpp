#include "Database.h"
#include <iostream>
#include <chrono>

namespace dfs {

Database::Database(const std::string& db_path) : m_db(nullptr) {
    if (sqlite3_open(db_path.c_str(), &m_db) != SQLITE_OK) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(m_db) << "\n";
    }
}

Database::~Database() {
    if (m_db) {
        sqlite3_close(m_db);
    }
}

bool Database::Execute(const std::string& sql) {
    char* errmsg = nullptr;
    if (sqlite3_exec(m_db, sql.c_str(), nullptr, nullptr, &errmsg) != SQLITE_OK) {
        std::cerr << "SQL error: " << errmsg << "\n";
        sqlite3_free(errmsg);
        return false;
    }
    return true;
}

bool Database::Init() {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::string sql_nodes = "CREATE TABLE IF NOT EXISTS Nodes ("
                            "node_id TEXT PRIMARY KEY, "
                            "ip TEXT, "
                            "port INTEGER, "
                            "last_heartbeat INTEGER, "
                            "capacity INTEGER, "
                            "used_space INTEGER);";

    std::string sql_files = "CREATE TABLE IF NOT EXISTS Files ("
                            "filename TEXT PRIMARY KEY, "
                            "size INTEGER);";

    std::string sql_chunks = "CREATE TABLE IF NOT EXISTS Chunks ("
                             "chunk_id TEXT PRIMARY KEY, "
                             "filename TEXT, "
                             "chunk_index INTEGER, "
                             "FOREIGN KEY(filename) REFERENCES Files(filename));";

    std::string sql_chunk_locs = "CREATE TABLE IF NOT EXISTS ChunkLocations ("
                                 "chunk_id TEXT, "
                                 "node_id TEXT, "
                                 "PRIMARY KEY(chunk_id, node_id), "
                                 "FOREIGN KEY(chunk_id) REFERENCES Chunks(chunk_id), "
                                 "FOREIGN KEY(node_id) REFERENCES Nodes(node_id));";

    return Execute(sql_nodes) && Execute(sql_files) && Execute(sql_chunks) && Execute(sql_chunk_locs);
}

bool Database::UpsertNode(const std::string& node_id, const std::string& ip, int port, int64_t capacity, int64_t used_space) {
    std::lock_guard<std::mutex> lock(m_mutex);
    auto now = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    
    std::string sql = "INSERT INTO Nodes (node_id, ip, port, last_heartbeat, capacity, used_space) "
                      "VALUES (?, ?, ?, ?, ?, ?) "
                      "ON CONFLICT(node_id) DO UPDATE SET "
                      "ip=excluded.ip, port=excluded.port, last_heartbeat=excluded.last_heartbeat, "
                      "capacity=excluded.capacity, used_space=excluded.used_space;";
                      
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, node_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, ip.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, port);
        sqlite3_bind_int64(stmt, 4, now);
        sqlite3_bind_int64(stmt, 5, capacity);
        sqlite3_bind_int64(stmt, 6, used_space);
        
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }
    return false;
}

std::vector<NodeInfo> Database::GetActiveNodes(int64_t current_time, int64_t timeout_sec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<NodeInfo> nodes;
    
    std::string sql = "SELECT node_id, ip, port, last_heartbeat, capacity, used_space FROM Nodes WHERE last_heartbeat >= ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, current_time - timeout_sec);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            NodeInfo n;
            n.node_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            n.ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            n.port = sqlite3_column_int(stmt, 2);
            n.last_heartbeat = sqlite3_column_int64(stmt, 3);
            n.capacity = sqlite3_column_int64(stmt, 4);
            n.used_space = sqlite3_column_int64(stmt, 5);
            nodes.push_back(n);
        }
        sqlite3_finalize(stmt);
    }
    return nodes;
}

std::vector<NodeInfo> Database::GetDeadNodes(int64_t current_time, int64_t timeout_sec) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<NodeInfo> nodes;
    
    std::string sql = "SELECT node_id, ip, port, last_heartbeat, capacity, used_space FROM Nodes WHERE last_heartbeat < ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int64(stmt, 1, current_time - timeout_sec);
        
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            NodeInfo n;
            n.node_id = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            n.ip = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            n.port = sqlite3_column_int(stmt, 2);
            n.last_heartbeat = sqlite3_column_int64(stmt, 3);
            n.capacity = sqlite3_column_int64(stmt, 4);
            n.used_space = sqlite3_column_int64(stmt, 5);
            nodes.push_back(n);
        }
        sqlite3_finalize(stmt);
    }
    return nodes;
}

void Database::RemoveNode(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string sql = "DELETE FROM Nodes WHERE node_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, node_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

bool Database::AddFile(const std::string& filename, int64_t size) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string sql = "INSERT INTO Files (filename, size) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int64(stmt, 2, size);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }
    return false;
}

bool Database::FileExists(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string sql = "SELECT count(*) FROM Files WHERE filename = ?;";
    sqlite3_stmt* stmt;
    bool exists = false;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = sqlite3_column_int(stmt, 0) > 0;
        }
        sqlite3_finalize(stmt);
    }
    return exists;
}

bool Database::AddChunk(const std::string& chunk_id, const std::string& filename, int chunk_index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string sql = "INSERT INTO Chunks (chunk_id, filename, chunk_index) VALUES (?, ?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, chunk_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, filename.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 3, chunk_index);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }
    return false;
}

bool Database::AddChunkLocation(const std::string& chunk_id, const std::string& node_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string sql = "INSERT OR IGNORE INTO ChunkLocations (chunk_id, node_id) VALUES (?, ?);";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, chunk_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, node_id.c_str(), -1, SQLITE_TRANSIENT);
        bool success = (sqlite3_step(stmt) == SQLITE_DONE);
        sqlite3_finalize(stmt);
        return success;
    }
    return false;
}

std::vector<std::string> Database::GetChunkLocations(const std::string& chunk_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> nodes;
    std::string sql = "SELECT node_id FROM ChunkLocations WHERE chunk_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, chunk_id.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            nodes.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }
    return nodes;
}

std::vector<std::string> Database::GetChunksForFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> chunks;
    std::string sql = "SELECT chunk_id FROM Chunks WHERE filename = ? ORDER BY chunk_index ASC;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, filename.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            chunks.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }
    return chunks;
}

std::vector<std::string> Database::GetChunksOnNode(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<std::string> chunks;
    std::string sql = "SELECT chunk_id FROM ChunkLocations WHERE node_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, node_id.c_str(), -1, SQLITE_TRANSIENT);
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            chunks.push_back(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0)));
        }
        sqlite3_finalize(stmt);
    }
    return chunks;
}

void Database::RemoveChunkLocationsOnNode(const std::string& node_id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string sql = "DELETE FROM ChunkLocations WHERE node_id = ?;";
    sqlite3_stmt* stmt;
    if (sqlite3_prepare_v2(m_db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, node_id.c_str(), -1, SQLITE_TRANSIENT);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

} // namespace dfs
