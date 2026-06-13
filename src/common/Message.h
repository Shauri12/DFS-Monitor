#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>
#include "SocketWrapper.h"

using json = nlohmann::json;

namespace dfs {

// Helper functions for sending/receiving JSON messages with a 4-byte length prefix

inline bool SendMessage(TCPSocket& sock, const json& j) {
    std::string data = j.dump();
    uint32_t length = data.size();
    
    // Send 4-byte length
    if (sock.Send(reinterpret_cast<const char*>(&length), sizeof(length)) != sizeof(length)) {
        return false;
    }
    
    // Send JSON string
    if (sock.Send(data) != length) {
        return false;
    }
    return true;
}

inline bool RecvMessage(TCPSocket& sock, json& j) {
    uint32_t length = 0;
    int bytes = sock.RecvExact(reinterpret_cast<char*>(&length), sizeof(length));
    if (bytes <= 0) return false;

    std::vector<char> buf(length);
    bytes = sock.RecvExact(buf.data(), length);
    if (bytes <= 0 || bytes != length) return false;

    std::string data(buf.begin(), buf.end());
    try {
        j = json::parse(data);
        return true;
    } catch (...) {
        return false;
    }
}

// For sending file chunks (binary data)
inline bool SendChunkData(TCPSocket& sock, const std::vector<char>& data) {
    uint32_t length = data.size();
    if (sock.Send(reinterpret_cast<const char*>(&length), sizeof(length)) != sizeof(length)) {
        return false;
    }
    if (length > 0) {
        if (sock.Send(data.data(), length) != length) return false;
    }
    return true;
}

inline bool RecvChunkData(TCPSocket& sock, std::vector<char>& data) {
    uint32_t length = 0;
    if (sock.RecvExact(reinterpret_cast<char*>(&length), sizeof(length)) <= 0) return false;
    
    data.resize(length);
    if (length > 0) {
        if (sock.RecvExact(data.data(), length) <= 0) return false;
    }
    return true;
}

} // namespace dfs
