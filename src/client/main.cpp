#include "Client.h"
#include "SocketWrapper.h"
#include <iostream>
#include <string>

using namespace dfs;

int main(int argc, char* argv[]) {
    if (!SocketSystem::Init()) {
        return 1;
    }

    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <mds_ip> <mds_port> <upload|download> <local_file> <remote_file>\n";
        return 1;
    }

    std::string mds_ip = argv[1];
    int mds_port = std::stoi(argv[2]);
    std::string action = argv[3];
    
    Client client(mds_ip, mds_port);

    if (action == "upload") {
        std::string local_file = argv[4];
        std::string remote_file = argv[5];
        client.UploadFile(local_file, remote_file);
    } else if (action == "download") {
        std::string remote_file = argv[4];
        std::string local_file = argv[5];
        client.DownloadFile(remote_file, local_file);
    } else {
        std::cerr << "Unknown action: " << action << "\n";
    }

    SocketSystem::Cleanup();
    return 0;
}
