#include "MetadataServer.h"
#include <iostream>

using namespace dfs;

int main(int argc, char* argv[]) {
    if (!SocketSystem::Init()) {
        return 1;
    }

    int port = 8080;
    if (argc > 1) {
        port = std::stoi(argv[1]);
    }

    MetadataServer mds(port, "metadata.db");
    mds.Start();

    std::cout << "Press Enter to stop..." << std::endl;
    std::cin.get();

    mds.Stop();
    SocketSystem::Cleanup();
    return 0;
}
