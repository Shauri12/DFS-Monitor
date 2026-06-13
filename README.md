# Distributed File System (DFS)

## 1. Summary
A highly resilient, multi-threaded Distributed File System (DFS) built from scratch in C++. This project simulates a large-scale storage architecture (similar to Hadoop or GFS) by splitting files into chunks, distributing them across multiple storage nodes, and guaranteeing high availability through automated replication and self-healing mechanisms. A beautiful real-time Web Dashboard provides visualizations of cluster health, active nodes, and storage distributions.

## 2. Tech Stack Used
### Backend (Core DFS)
- **C++17**: Core systems programming.
- **TCP Sockets (Winsock2)**: Raw network communication and binary data streaming.
- **POSIX / `std::thread`**: Multithreading for parallel downloads and concurrent client handling.
- **SQLite3**: Robust, concurrent metadata storage.
- **nlohmann/json**: Serialization and deserialization of network protocols.
- **CMake**: Build system management.

### Web Dashboard
- **Frontend**: React.js, Vite, Vanilla CSS (Premium Glassmorphism & Animations).
- **Backend API**: Node.js, Express.js.
- **Integration**: Real-time SQLite querying from Node to the C++ Metadata Database.

## 3. Project Structure
```text
📦 OS Project
 ┣ 📂 src
 ┃ ┣ 📂 client            # C++ Client for file chunking, parallel uploads/downloads
 ┃ ┣ 📂 common            # Shared TCP Socket wrappers and JSON Message protocols
 ┃ ┣ 📂 metadata_server   # C++ Brain mapping chunks, heartbeats, and self-healing
 ┃ ┗ 📂 storage_node      # C++ Daemon for persisting binary chunks to local disk
 ┣ 📂 dashboard
 ┃ ┣ 📂 backend           # Node.js/Express API bridging SQLite to the frontend
 ┃ ┗ 📂 frontend          # React/Vite UI for real-time monitoring
 ┣ 📂 third_party         # SQLite3 and JSON dependencies
 ┗ 📜 CMakeLists.txt      # Root build configuration
```

## 4. Features and Workflow
1. **Metadata Management Service**: Maintains a global namespace, chunk maps, and node locations in SQLite.
2. **Chunk-Based Distributed Storage**: The client splits large files into 1MB chunks and streams them directly to storage nodes.
3. **Replication Engine**: Maintains a Replication Factor of 3. Every chunk is stored on 3 distinct, least-loaded nodes.
4. **Heartbeat Monitoring**: Storage nodes ping the Metadata Server every 5 seconds to report health and capacity.
5. **Automatic Re-Replication (Self-Healing)**: If a node misses heartbeats for >15 seconds, it is marked DEAD. The Metadata Server automatically commands surviving nodes to duplicate lost chunks onto healthy nodes to restore the replication factor.
6. **Parallel File Retrieval**: The Client spawns multiple threads to fetch chunks from different replica nodes concurrently, maximizing download speeds.
7. **Intelligent Load-Aware Placement**: Chunks are dynamically assigned to nodes with the lowest storage utilization to prevent hotspots.
8. **Real-Time Monitoring**: React Dashboard polls the cluster to visualize node failures and capacity changes instantly.

## 5. Installation and Setup

### Compiling the C++ Core
Ensure you have CMake and MinGW (GCC/G++) installed.
```bash
mkdir build
cd build
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ..
mingw32-make
```

### Running the Cluster
Open 3 separate terminals in the `build` directory:
```bash
# Terminal 1: Start Metadata Server
.\src\metadata_server\metadata_server.exe 8080

# Terminal 2: Start 3 Storage Nodes
start .\src\storage_node\storage_node.exe node1 127.0.0.1 9001 127.0.0.1 8080
start .\src\storage_node\storage_node.exe node2 127.0.0.1 9002 127.0.0.1 8080
start .\src\storage_node\storage_node.exe node3 127.0.0.1 9003 127.0.0.1 8080

# Terminal 3: Upload and Download using the Client
fsutil file createnew my_test.mp4 5242880
.\src\client\client.exe 127.0.0.1 8080 upload my_test.mp4 remote.mp4
.\src\client\client.exe 127.0.0.1 8080 download remote.mp4 retrieved.mp4
```

### Running the Web Dashboard
```bash
# Terminal 4: Start Node.js API
cd dashboard/backend
npm install
node server.js

# Terminal 5: Start React App
cd dashboard/frontend
npm install
npm run dev
```
Navigate to `http://localhost:5173` in your browser.

## 6. Developer
- **Developer**: Shaurya Bandari
- **Email**: shaurya170705@gmail.com
