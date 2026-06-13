#include "StorageNode.h"
#include <iostream>

using namespace dfs;

int main(int argc, char* argv[]) {
    if (!SocketSystem::Init()) {
        return 1;
    }

    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <node_id> <ip> <port> <mds_ip> <mds_port> <storage_dir>\n";
        return 1;
    }

    std::string node_id = argv[1];
    std::string ip = argv[2];
    int port = std::stoi(argv[3]);
    std::string mds_ip = argv[4];
    int mds_port = std::stoi(argv[5]);
    std::string storage_dir = (argc > 6) ? argv[6] : ("storage_" + node_id);

    StorageNode node(node_id, ip, port, mds_ip, mds_port, storage_dir);
    node.Start();

    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();

    node.Stop();
    SocketSystem::Cleanup();
    return 0;
}
