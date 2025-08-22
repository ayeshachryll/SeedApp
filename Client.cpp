#include "client.h"
#include "seed.h"
#include <sys/stat.h>
#include <iomanip>

void *client::download_file(void *arg)
{
    int file_id = *(int *)arg;
    delete (int *)arg;

    // if (seed::available_files.find(file_id) == seed::available_files.end()) {
    //     std::cout << "No seeders for file ID" << file_id;
    //     return nullptr;
    // }

    std::string filename = seed::available_files[file_id].first;

    for (int target_port : seed::ports)
    {
        if (target_port == seed::listen_port)
        {
            continue;
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("Failed to create socket");
            continue;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(target_port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        bool has_file = false;
        if (connect(sock, (sockaddr *)&addr, sizeof(addr)) == 0)
        {
            send(sock, "LIST", 4, 0);
            char buffer[seed::buffer_size];
            ssize_t n = recv(sock, buffer, sizeof(buffer), 0);

            if (n > 0)
            {
                buffer[n] = '\0';
                std::string data(buffer);
                std::string search_file = "[" + std::to_string(file_id) + "] " + filename;

                if (data.find(search_file) != std::string::npos)
                {
                    has_file = true;
                    seed::current_downloads[file_id] = filename;
                }
            }
        }
        close(sock);

        if (!has_file)
        {
            continue;
        }

        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            perror("Failed to create socket");
            continue;
        }

        addr.sin_family = AF_INET;
        addr.sin_port = htons(target_port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(sock, (sockaddr *)&addr, sizeof(addr)) == 0)
        {
            std::string request = "DOWNLOAD " + std::to_string(file_id);
            send(sock, request.c_str(), request.size(), 0);

            std::string dir_path = "files/" + std::to_string(file_id);
            mkdir(dir_path.c_str(), 0700);

            std::string file_path = dir_path + "/" + filename;
            FILE *out = fopen(file_path.c_str(), "wb");

            if (!out)
            {
                close(sock);
            }

            char buffer[32];
            ssize_t n;
            long total_bytes = 0;
            bool success = true;

            while ((n = recv(sock, buffer, 32, 0)) > 0)
            {
                if (fwrite(buffer, 1, n, out) != n)
                {
                    success = false;
                    break;
                }
                total_bytes += n;
                seed::bytes_downloaded = total_bytes;
                sleep(10);
            }

            fclose(out);
            close(sock);

            if (success)
            {
                return nullptr;
            }
            else
            {
                std::cout << "Download failed. Lost connection.\n";
            }
        }
        else
        {
            close(sock);
        }
    }

    perror("Failed to download from any source.");
    return nullptr;
}

void *client::request_files(void *arg)
{
    int port = *(int *)arg;
    delete (int *)arg;

    if (port == seed::listen_port)
    {
        return nullptr;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror("Failed to create socket");
        return nullptr;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(sock, (sockaddr *)&addr, sizeof(addr)) == 0)
    {
        send(sock, "LIST", 4, 0);
        char buffer[seed::buffer_size];
        ssize_t n = recv(sock, buffer, sizeof(buffer), 0);

        if (n > 0)
        {
            buffer[n] = '\0';
            std::string data(buffer);
            size_t pos = 0;

            {
                std::lock_guard<std::mutex> lock(seed::files_mutex);

                while ((pos = data.find('\n')) != std::string::npos)
                {
                    std::string f = data.substr(0, pos);
                    data.erase(0, pos + 1);

                    size_t bracket_pos = f.find("] ");
                    if (bracket_pos != std::string::npos && f[0] == '[')
                    {
                        int key = std::stoi(f.substr(1, bracket_pos - 1));
                        std::string rest = f.substr(bracket_pos + 2);
                        size_t paren_start = rest.find(" (");
                        std::string filename = rest.substr(0, paren_start);
                        size_t size_start = rest.find("(") + 1;
                        size_t size_end = rest.find(" bytes)");
                        long file_size = std::stol(rest.substr(size_start, size_end - size_start));

                        std::string local_path = "files/" + std::to_string(key) + "/" + filename;
                        FILE *local_file = fopen(local_path.c_str(), "rb");

                        if (local_file)
                        {
                            fclose(local_file);
                            continue;
                        }

                        seed::available_files[key] = std::make_pair(filename, file_size);
                    }
                }
            }
        }
    }

    close(sock);
    return nullptr;
}

void *client::download_status(void *arg)
{
    std::cout << "Download status:\n";

    {
        std::lock_guard<std::mutex> lock(seed::files_mutex);

        for (const auto &entry : seed::current_downloads)
        {
            int file_id = entry.first;
            const std::string &filename = entry.second;

            long total_size = 0;
            if (seed::available_files.find(file_id) != seed::available_files.end())
            {
                total_size = seed::available_files[file_id].second;
            }

            double downloaded_kb = static_cast<double>(seed::bytes_downloaded) / 1024.0;
            double total_kb = static_cast<double>(total_size) / 1024.0;
            double percentage = 0.0;

            if (total_size > 0)
            {
                percentage = (static_cast<double>(seed::bytes_downloaded) / total_size) * 100.0;
            }

            std::cout << "[" << file_id << "] " << filename << " - "
                      << std::fixed << std::setprecision(2) << downloaded_kb
                      << " / " << total_kb << " KB ("
                      << std::setprecision(1) << percentage << "%)\n";
        }
    }

    sleep(1);

    return nullptr;
}