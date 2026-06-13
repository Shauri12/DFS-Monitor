#pragma once

#include <string>
#include <vector>

namespace dfs {

class Client {
public:
    Client(const std::string& mds_ip, int mds_port);

    bool UploadFile(const std::string& local_filepath, const std::string& remote_filename);
    bool DownloadFile(const std::string& remote_filename, const std::string& local_filepath);

private:
    std::string m_mds_ip;
    int m_mds_port;
    
    const size_t CHUNK_SIZE = 1024 * 1024; // 1 MB chunks
};

} // namespace dfs
