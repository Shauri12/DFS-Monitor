#pragma once

#include <string>
#include <vector>
#include <memory>
#include <iostream>

#ifdef _WIN32
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0600
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
using socket_t = SOCKET;
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
using socket_t = int;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

namespace dfs {

class SocketSystem {
public:
    static bool Init() {
#ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            std::cerr << "WSAStartup failed.\n";
            return false;
        }
#endif
        return true;
    }

    static void Cleanup() {
#ifdef _WIN32
        WSACleanup();
#endif
    }
};

class TCPSocket {
public:
    TCPSocket();
    explicit TCPSocket(socket_t sock);
    ~TCPSocket();

    // Disable copy
    TCPSocket(const TCPSocket&) = delete;
    TCPSocket& operator=(const TCPSocket&) = delete;

    // Enable move
    TCPSocket(TCPSocket&& other) noexcept;
    TCPSocket& operator=(TCPSocket&& other) noexcept;

    bool Bind(int port);
    bool Listen(int backlog = SOMAXCONN);
    std::unique_ptr<TCPSocket> Accept();
    bool Connect(const std::string& ip, int port);

    int Send(const std::string& data);
    int Send(const char* buf, size_t len);
    int Recv(std::string& data, size_t max_len = 4096);
    int RecvExact(char* buf, size_t len);

    void Close();

    bool IsValid() const { return m_sock != INVALID_SOCKET; }

private:
    socket_t m_sock;
};

} // namespace dfs
