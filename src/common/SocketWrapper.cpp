#include "SocketWrapper.h"
#include <iostream>
#include <cstring>

namespace dfs {

TCPSocket::TCPSocket() {
    m_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_sock == INVALID_SOCKET) {
        std::cerr << "Failed to create socket.\n";
    }
}

TCPSocket::TCPSocket(socket_t sock) : m_sock(sock) {}

TCPSocket::~TCPSocket() {
    Close();
}

TCPSocket::TCPSocket(TCPSocket&& other) noexcept : m_sock(other.m_sock) {
    other.m_sock = INVALID_SOCKET;
}

TCPSocket& TCPSocket::operator=(TCPSocket&& other) noexcept {
    if (this != &other) {
        Close();
        m_sock = other.m_sock;
        other.m_sock = INVALID_SOCKET;
    }
    return *this;
}

bool TCPSocket::Bind(int port) {
    if (m_sock == INVALID_SOCKET) return false;

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(m_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed.\n";
        return false;
    }
    return true;
}

bool TCPSocket::Listen(int backlog) {
    if (m_sock == INVALID_SOCKET) return false;
    if (listen(m_sock, backlog) == SOCKET_ERROR) {
        std::cerr << "Listen failed.\n";
        return false;
    }
    return true;
}

std::unique_ptr<TCPSocket> TCPSocket::Accept() {
    if (m_sock == INVALID_SOCKET) return nullptr;

    sockaddr_in client_addr;
#ifdef _WIN32
    int client_len = sizeof(client_addr);
#else
    socklen_t client_len = sizeof(client_addr);
#endif

    socket_t client_sock = accept(m_sock, (sockaddr*)&client_addr, &client_len);
    if (client_sock == INVALID_SOCKET) {
        return nullptr;
    }
    return std::make_unique<TCPSocket>(client_sock);
}

bool TCPSocket::Connect(const std::string& ip, int port) {
    if (m_sock == INVALID_SOCKET) return false;

    sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if (connect(m_sock, (sockaddr*)&server_addr, sizeof(server_addr)) == SOCKET_ERROR) {
        return false;
    }
    return true;
}

int TCPSocket::Send(const std::string& data) {
    return Send(data.c_str(), data.length());
}

int TCPSocket::Send(const char* buf, size_t len) {
    if (m_sock == INVALID_SOCKET) return -1;
    int bytes_sent = send(m_sock, buf, len, 0);
    return bytes_sent;
}

int TCPSocket::Recv(std::string& data, size_t max_len) {
    if (m_sock == INVALID_SOCKET) return -1;
    char* buf = new char[max_len];
    int bytes_received = recv(m_sock, buf, max_len, 0);
    if (bytes_received > 0) {
        data.assign(buf, bytes_received);
    }
    delete[] buf;
    return bytes_received;
}

int TCPSocket::RecvExact(char* buf, size_t len) {
    if (m_sock == INVALID_SOCKET) return -1;
    size_t total_received = 0;
    while (total_received < len) {
        int bytes = recv(m_sock, buf + total_received, len - total_received, 0);
        if (bytes <= 0) return bytes; // error or closed
        total_received += bytes;
    }
    return total_received;
}

void TCPSocket::Close() {
    if (m_sock != INVALID_SOCKET) {
#ifdef _WIN32
        closesocket(m_sock);
#else
        close(m_sock);
#endif
        m_sock = INVALID_SOCKET;
    }
}

} // namespace dfs
