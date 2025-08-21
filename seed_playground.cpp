#include <iostream>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <thread>
#include <mutex>
#include <map>
#include <fstream>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>
// #include "server.h"

const int num_ports = 5;
const int buffer_size = 1024;
const int ports[num_ports] = {8999, 9000, 9002, 9003, 9004};
int listen_fd, listen_port;
std::string directory_path = "./files";
std::map<int, std::string> available_files; // Just store file_id -> filename
std::mutex files_mutex;

std::map<int, std::string> list_files()
{
    std::map<int, std::string> map_files;
    DIR *dir = opendir(directory_path.c_str());

    if (dir == NULL)
    {
        perror("Error opening directory");
        exit(EXIT_FAILURE);
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL)
    {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
        {
            continue;
        }

        std::string full_path = directory_path + "/" + name;

        if (entry->d_type == DT_DIR)
        {
            DIR *subdir = opendir(full_path.c_str());

            if (subdir == NULL)
            {
                perror("Error opening directory");
                exit(EXIT_FAILURE);
            }

            struct dirent *subentry;

            while ((subentry = readdir(subdir)) != NULL)
            {
                std::string file = subentry->d_name;
                if (file == "." || file == "..")
                {
                    continue;
                }

                if (subentry->d_type == DT_REG)
                {
                    int key = std::stoi(name);
                    map_files[key] = file;
                }
            }

            closedir(subdir);
        }
    }

    closedir(dir);
    return map_files;
}

void *handle_client_thread(void *arg)
{
    int client_fd = *(int *)arg;
    delete (int *)arg;

    char buffer[buffer_size];
    ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
    if (bytes <= 0)
    {
        close(client_fd);
        return nullptr;
    }
    buffer[bytes] = '\0';
    std::string request(buffer);

    if (request == "LIST")
    {
        auto files = list_files();

        std::string response;
        for (auto &f : files)
        {
            response += "[" + std::to_string(f.first) + "] " + f.second + "\n";
        }
        send(client_fd, response.c_str(), response.size(), 0);
    }
    else if (request.find("DOWNLOAD ") != std::string::npos)
    {
        std::string file_id = request.substr(9);
        std::string file_path = directory_path + "/" + file_id;

        DIR *dir = opendir(file_path.c_str());

        if (dir == NULL)
        {
            perror("Error opening directory");
            exit(EXIT_FAILURE);
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL)
        {
            std::string name = entry->d_name;
            if (name == "." || name == "..")
            {
                continue;
            }

            if (entry->d_type == DT_REG)
            {
                file_path += "/" + name;
                break;
            }
        }

        closedir(dir);

        FILE *file = fopen(file_path.c_str(), "rb");
        if (!file)
        {
            std::cerr << "Failed to open file\n";
            close(client_fd);
            return nullptr;
        }

        char buffer_file[32];
        size_t bytes_read;
        while ((bytes_read = fread(buffer_file, 1, 32, file)) > 0)
        {
            if (send(client_fd, buffer_file, bytes_read, 0) == -1)
            {
                perror("Error sending file");
                break;
            }
        }
        fclose(file);
    }

    close(client_fd);
    return nullptr;
}

void *listener_thread(void *arg)
{
    struct sockaddr_in addr{};
    socklen_t addrlen = sizeof(addr);

    while (1)
    {
        int new_socket = accept(listen_fd, (struct sockaddr *)&addr, &addrlen);
        if (new_socket < 0)
        {
            perror("Accept failed");
            continue;
        }
        else
        {
            int *fd_ptr = new int(new_socket);
            pthread_t handler;
            pthread_create(&handler, nullptr, handle_client_thread, fd_ptr);
            pthread_detach(handler);
        }
    }

    return nullptr;
}

void *download_file(void *arg)
{
    int file_id = *(int *)arg;
    delete (int *)arg;

    if (available_files.find(file_id) == available_files.end())
    {
        perror("File ID not on list.");
        return nullptr;
    }

    std::string filename = available_files[file_id];

    // Attempt download from available sources
    for (int target_port : ports)
    {
        if (target_port == listen_port)
        {
            continue;
        }

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            continue;
        }

        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(target_port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        // Check if this port has the file (quick verification)
        bool has_file = false;
        if (connect(sock, (sockaddr *)&addr, sizeof(addr)) == 0)
        {
            send(sock, "LIST", 4, 0);
            char buffer[buffer_size];
            ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
            if (n > 0)
            {
                buffer[n] = '\0';
                std::string data(buffer);
                std::string search_pattern = "[" + std::to_string(file_id) + "] " + filename;
                if (data.find(search_pattern) != std::string::npos)
                {
                    has_file = true;
                }
            }
        }
        close(sock);

        if (!has_file)
        {
            continue;
        }

        // Try to download from this port
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (sock < 0)
        {
            continue;
        }
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
                continue;
            }

            char buffer[32];
            ssize_t n;
            int total_bytes = 0;
            bool success = true;

            while ((n = recv(sock, buffer, 32, 0)) > 0)
            {
                if (fwrite(buffer, 1, n, out) != n)
                {
                    success = false;
                    break;
                }
                total_bytes += n;
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

void *request_files(void *arg)
{
    int port = *(int *)arg;
    delete (int *)arg;

    if (port == listen_port)
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
        char buffer[buffer_size];
        ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
        if (n > 0)
        {
            buffer[n] = '\0';
            std::string data(buffer);
            size_t pos = 0;

            {
                std::lock_guard<std::mutex> lock(files_mutex);

                while ((pos = data.find('\n')) != std::string::npos)
                {
                    std::string f = data.substr(0, pos);
                    data.erase(0, pos + 1);

                    size_t bracket_pos = f.find("] ");
                    if (bracket_pos != std::string::npos && f[0] == '[')
                    {
                        int key = std::stoi(f.substr(1, bracket_pos - 1));
                        std::string filename = f.substr(bracket_pos + 2);

                        std::string local_path = "files/" + std::to_string(key) + "/" + filename;
                        FILE *local_file = fopen(local_path.c_str(), "rb");
                        if (local_file)
                        {
                            fclose(local_file);
                            continue;
                        }

                        available_files[key] = filename;
                    }
                }
            }
        }
    }
    close(sock);
    return nullptr;
}

bool bind_available(int &out_fd, int &out_port)
{
    int opt = 1;
    struct sockaddr_in addr{};

    for (int i = 0; i < num_ports; ++i)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);

        if (fd == -1)
        {
            perror("Socket creation failed");
            exit(EXIT_FAILURE);
        }

        if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)))
        {
            perror("setsockopt failed");
            exit(EXIT_FAILURE);
        }

        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
        addr.sin_port = htons(ports[i]);

        if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            listen_fd = fd;
            listen_port = ports[i];

            if (listen(listen_fd, 4) < 0)
            {
                perror("Listen failed");
                close(listen_fd);
                exit(EXIT_FAILURE);
            }

            return true;
        }

        close(fd);
    }
    return false;
}

void print_menu()
{
    std::cout << "\nSeed App\n";
    std::cout << "[1] List available files.\n";
    std::cout << "[2] Download file.\n";
    std::cout << "[3] Download status.\n";
    std::cout << "[4] Exit.\n";
    std::cout << "\n? ";
}

int main()
{
    std::cout << "Finding available ports...";
    if (!bind_available(listen_fd, listen_port))
    {
        std::cerr << "No available ports found. Exiting.\n";
        return 1;
    }

    std::cout << "Found port " << listen_port << "\n";
    std::cout << "Listening at port " << listen_port << "\n";

    pthread_t listener;
    pthread_create(&listener, nullptr, listener_thread, &listen_fd);

    while (1)
    {
        print_menu();
        int choice = 0;

        if (!(std::cin >> choice))
        {
            std::cout << "Invalid choice. Enter a valid number.\n";
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            continue;
        }
        else if (choice == 1)
        {
            std::cout << "\nSearching for files...";
            available_files.clear();

            std::vector<pthread_t> threads;

            for (int p : ports)
            {
                if (p != listen_port)
                {
                    pthread_t thread;
                    int *port_ptr = new int(p);
                    pthread_create(&thread, nullptr, request_files, port_ptr);
                    threads.push_back(thread);
                }
            }

            for (pthread_t &thread : threads)
            {
                pthread_join(thread, nullptr);
            }

            std::cout << " done.\n";

            if (available_files.empty())
            {
                std::cout << "No files available.\n";
            }
            else
            {
                std::cout << "Files available:\n";
                for (const auto &entry : available_files)
                {
                    int file_id = entry.first;
                    const std::string &filename = entry.second;

                    std::cout << "[" << file_id << "] " << filename << "\n";
                }
            }
        }
        else if (choice == 2)
        {
            int file_id;
            std::cout << "\nEnter file ID: ";
            std::cin >> file_id;

            if (available_files.find(file_id) == available_files.end())
            {
                std::cout << "File not found.\n";
            }
            else
            {
                std::cout << "Download started. File: [" << file_id << "] " << available_files[file_id] << "\n";
                std::cout << "Scanning for available sources...\n";

                // Count sources before starting download
                std::string filename = available_files[file_id];
                int sources_count = 0;

                for (int target_port : ports)
                {
                    if (target_port == listen_port)
                    {
                        continue;
                    }

                    int sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (sock < 0)
                    {
                        continue;
                    }

                    sockaddr_in addr{};
                    addr.sin_family = AF_INET;
                    addr.sin_port = htons(target_port);
                    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

                    if (connect(sock, (sockaddr *)&addr, sizeof(addr)) == 0)
                    {
                        send(sock, "LIST", 4, 0);
                        char buffer[buffer_size];
                        ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
                        if (n > 0)
                        {
                            buffer[n] = '\0';
                            std::string data(buffer);
                            std::string search_pattern = "[" + std::to_string(file_id) + "] " + filename;
                            if (data.find(search_pattern) != std::string::npos)
                            {
                                sources_count++;
                            }
                        }
                    }
                    close(sock);
                }

                std::cout << "Found " << sources_count << " seeder/s.\n";

                if (sources_count == 0)
                {
                    std::cout << "File not found.\n";
                }
                else
                {
                    std::cout << "Starting download...\n";
                    int *file_id_ptr = new int(file_id);
                    pthread_t download_thread;
                    pthread_create(&download_thread, nullptr, download_file, file_id_ptr);
                    pthread_detach(download_thread);
                }
            }
        }

        else if (choice == 3)
        {
            // download status
        }
        else if (choice == 4)
        {
            std::cout << "Exiting...\n";
            // free pointers and close everything
            break;
        }
        else
        {
            std::cout << "Invalid choice. Enter a valid number.\n";
        }
    }

    return 0;
}