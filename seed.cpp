#include "seed.h"
#include "server.h"
#include "client.h"
#include <limits>
#include <iomanip>

const int seed::ports[seed::num_ports] = {8999, 9000, 9002, 9003, 9004};
int seed::listen_fd;
int seed::listen_port;
std::string seed::directory_path = "./files";
std::map<int, std::pair<std::string, long>> seed::available_files;
std::map<int, std::string> seed::current_downloads;
long seed::bytes_downloaded;
long seed::file_size;
std::mutex seed::files_mutex;

void seed::print_menu()
{
    std::cout << "\nSeed App\n";
    std::cout << "[1] List available files.\n";
    std::cout << "[2] Download file.\n";
    std::cout << "[3] Download status.\n";
    std::cout << "[4] Exit.\n";
    std::cout << "\n? ";
}

int seed::count_sources(int file_id, const std::string &filename)
{
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

    return sources_count;
}

int seed::run()
{
    std::cout << "Finding available ports...";

    if (!server::bind_available(listen_fd, listen_port))
    {
        std::cerr << "No available ports found. Exiting.\n";
        return 1;
    }

    std::cout << "Found port " << listen_port << "\n";
    std::cout << "Listening at port " << listen_port << "\n";

    pthread_t accept;
    pthread_create(&accept, nullptr, server::accept_thread, &listen_fd);

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
                    pthread_create(&thread, nullptr, client::request_files, port_ptr);
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
                    const std::string &filename = entry.second.first;
                    long file_size = entry.second.second;

                    // Convert to KB
                    double size_kb = static_cast<double>(file_size) / 1024.0;

                    std::cout << "[" << file_id << "] " << filename
                              << " (" << std::fixed << std::setprecision(2) << size_kb << " KB)\n";
                }
            }
        }
        else if (choice == 2)
        {
            int file_id;
            std::cout << "\nEnter file ID: ";
            std::cin >> file_id;

            std::cout << "Locating seeders...";
            if (available_files.find(file_id) == available_files.end())
            {
                std::cout << "Failed.\n";
                std::cout << "No seeders for file ID " << file_id << "\n";
            }
            else
            {
                // to change: for each port, find the file. spawn new download_file threads according to ports that have the file.

                int seeders_count = count_sources(file_id, available_files[file_id].first);
                std::cout << "Found " << seeders_count << " seeder/s.\n";
                std::cout << "Download started. File: [" << file_id << "] " << available_files[file_id].first << "\n";

                std::cout << "Starting download...\n";
                int *file_id_ptr = new int(file_id);
                pthread_t download_thread;
                pthread_create(&download_thread, nullptr, client::download_file, file_id_ptr);
            }
        }

        else if (choice == 3)
        {
            if (current_downloads.empty())
            {
                std::cout << "No active downloads.\n";
            }
            else
            {
                pthread_t status_thread;
                pthread_create(&status_thread, nullptr, client::download_status, nullptr);
                pthread_detach(status_thread);
            }
        }
        else if (choice == 4)
        {
            std::cout << "Exiting...\n";
            break;
        }
        else
        {
            std::cout << "Invalid choice. Enter a valid number.\n";
        }
    }

    return 0;
}

int main()
{
    return seed::run();
}

// #include <iostream>
// #include <algorithm>
// #include <vector>
// #include <string>
// #include <cstring>
// #include <thread>
// #include <mutex>
// #include <map>
// #include <fstream>
// #include <unistd.h>
// #include <arpa/inet.h>
// #include <pthread.h>
// #include <dirent.h>
// // #include "server.h"

// const int num_ports = 5;
// const int buffer_size = 1024;
// const int ports[num_ports] = {8999, 9000, 9002, 9003, 9004};
// int listen_fd, listen_port;
// std::string directory_path = "./files";
// std::map<int, std::pair<int, std::string>> available_files;

// // void* handle_receive(void* arg) {
// //     int client_socket = *(int*)arg;
// //     char buffer[1024];
// //     while (true) {
// //         memset(buffer, 0, sizeof(buffer));
// //         int bytes_received = recv(client_socket, buffer, sizeof(buffer), 0);
// //         if (bytes_received <= 0) {
// //             std::cout << "Client disconnected.\n";
// //             break;
// //         }
// //         std::cout << "Client: " << buffer << std::endl;
// //     }

// //     return nullptr;
// // }

// // void* handle_send(void* arg) {
// //     int client_socket = *(int*)arg;
// //     std::string message;
// //     while (true) {
// //         std::getline(std::cin, message);
// //         send(client_socket, message.c_str(), message.size(), 0);
// //     }

// //     return nullptr;
// // }

// std::map<int, std::string> list_files() {
//     std::map<int, std::string> map_files;
//     DIR *dir = opendir(directory_path.c_str());

//     if (dir == NULL) {
//         perror("Error opening directory");
//         exit(EXIT_FAILURE);
//     }

//     struct dirent *entry;
//     while ((entry = readdir(dir)) != NULL) {
//         std::string name = entry->d_name;
//         if (name == "." || name == "..") {
//             continue;
//         }

//         std::string full_path = directory_path + "/" + name;

//         if (entry->d_type == DT_DIR) {
//             DIR* subdir = opendir(full_path.c_str());

//             if (subdir == NULL) {
//                 perror("Error opening directory");
//                 exit(EXIT_FAILURE);
//             }

//             struct dirent *subentry;

//             while((subentry = readdir(subdir)) != NULL){
//                 std::string file = subentry->d_name;
//                 if(file == "." || file == ".."){
//                     continue;
//                 }

//                 if(subentry->d_type == DT_REG){
//                     int key = std::stoi(name);
//                     map_files[key] = file;
//                 }
//             }

//             closedir(subdir);
//         }
//     }

//     closedir(dir);
//     return map_files;
// }

// void* handle_client_thread(void* arg) {
//     int client_fd = *(int*)arg;
//     delete (int*)arg;

//     char buffer[buffer_size];
//     ssize_t bytes = recv(client_fd, buffer, sizeof(buffer), 0);
//     if (bytes <= 0) {
//         close(client_fd);
//         return nullptr;
//     }
//     buffer[bytes] = '\0';
//     std::string request(buffer);

//     if (request == "LIST") {
//         auto files = list_files();

//         std::string response;
//         for (auto& f : files) {
//             response += "[" + std::to_string(f.first) + "] " + f.second + "\n";
//         }
//         send(client_fd, response.c_str(), response.size(), 0);
//     }
//     else if (request.find("DOWNLOAD ") != std::string::npos) {
//         std::string file_id = request.substr(9);
//         std::string file_path = directory_path + "/" + file_id;

//         DIR *dir = opendir(file_path.c_str());

//         if (dir == NULL)
//         {
//             perror("Error opening directory");
//             exit(EXIT_FAILURE);
//         }

//         struct dirent *entry;
//         while ((entry = readdir(dir)) != NULL)
//         {
//             std::string name = entry->d_name;
//             if (name == "." || name == "..") {
//                 continue;
//             }

//             if (entry->d_type == DT_REG) {
//                 file_path += "/" + name;
//                 break;
//             }
//         }

//         closedir(dir);

//         FILE *file = fopen(file_path.c_str(), "rb");
//         if (!file) {
//             std::cerr << "Failed to open file\n";
//             close(client_fd);
//             return nullptr;
//         }

//         char buffer_file[32];
//         size_t bytes_read;
//         while ((bytes_read = fread(buffer_file, 1, 32, file)) > 0) {
//             if (send(client_fd, buffer_file, bytes_read, 0) == -1)
//             {
//                 perror("Error sending file");
//                 break;
//             }
//         }
//         fclose(file);
//     }

//     close(client_fd);
//     return nullptr;
// }

// void* listener_thread(void* arg) {
//     struct sockaddr_in addr{};
//     socklen_t addrlen = sizeof(addr);

//     while(1){
//         int new_socket = accept(listen_fd, (struct sockaddr*)&addr, &addrlen);
//         if (new_socket < 0) {
//             perror("Accept failed");
//             exit(EXIT_FAILURE);
//         } else {
//             int* fd_ptr = new int(new_socket);
//             pthread_t handler;
//             pthread_create(&handler, nullptr, handle_client_thread, fd_ptr);
//             pthread_detach(handler);
//         }
//     }

//     return nullptr;
// }

// void *download_file(void *arg) {
//     int file_id = *(int *)arg;
//     delete (int *)arg;

//     if (available_files.find(file_id) == available_files.end()) {
//         perror("File ID not on list.");
//         return nullptr;
//     }

//     int target_port = available_files[file_id].first;
//     std::string filename = available_files[file_id].second;

//     int sock = socket(AF_INET, SOCK_STREAM, 0);
//     if (sock < 0) {
//         perror("Socket creation failed.");
//         return nullptr;
//     }

//     sockaddr_in addr{};
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(target_port);
//     inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

//     if (connect(sock, (sockaddr *)&addr, sizeof(addr)) == 0) {
//         std::string request = "DOWNLOAD " + std::to_string(file_id);
//         send(sock, request.c_str(), request.size(), 0);

//         FILE *out = fopen(("files/" + filename).c_str(), "wb");
//         if (!out) {
//             perror("File not found.");
//             close(sock);
//             return nullptr;
//         }

//         char buffer[32];
//         ssize_t n;
//         int total_bytes = 0;
//         while ((n = recv(sock, buffer, 32, 0)) > 0) {
//             fwrite(buffer, 1, n, out);
//             total_bytes += n;
//         }

//         fclose(out);
//     }
//     close(sock);
//     return nullptr;
// }

// std::vector<std::string> request_files(int port) {
//     std::vector<std::string> files;
//     int sock = socket(AF_INET, SOCK_STREAM, 0);

//     sockaddr_in addr{};
//     addr.sin_family = AF_INET;
//     addr.sin_port = htons(port);
//     inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

//     if (connect(sock, (sockaddr*)&addr, sizeof(addr)) == 0 && port != listen_port) {
//         send(sock, "LIST", 4, 0);
//         char buffer[buffer_size];
//         ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
//         if (n > 0) {
//             buffer[n] = '\0';
//             std::string data(buffer);
//             size_t pos = 0;
//             while ((pos = data.find('\n')) != std::string::npos) {
//                 files.push_back(data.substr(0, pos));
//                 data.erase(0, pos+1);
//             }
//         }
//     }

//     close(sock);
//     return files;
// }

// bool bind_available(int& out_fd, int& out_port) {
//     int opt = 1;
//     struct sockaddr_in addr{};

//     for (int i = 0; i < num_ports; ++i) {
//         int fd = socket(AF_INET, SOCK_STREAM, 0);

//         if (fd == -1){
//             perror("Socket creation failed");
//             exit(EXIT_FAILURE);
//         }

//         if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
//             perror("setsockopt failed");
//             exit(EXIT_FAILURE);
//         }

//         addr.sin_family = AF_INET;
//         addr.sin_addr.s_addr = htonl(INADDR_ANY);
//         addr.sin_port = htons(ports[i]);

//         if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
//             listen_fd = fd;
//             listen_port = ports[i];

//             if(listen(listen_fd, 4) < 0){
//                 perror("Listen failed");
//                 close(listen_fd);
//                 exit(EXIT_FAILURE);
//             }

//             return true;
//         }

//         close(fd);
//     }
//     return false;
// }

// void print_menu() {
//     std::cout << "\nSeed App\n";
//     std::cout << "[1] List available files.\n";
//     std::cout << "[2] Download file.\n";
//     std::cout << "[3] Download status.\n";
//     std::cout << "[4] Exit.\n";
//     std::cout << "\n? ";
// }

// int main() {
//     std::cout << "Finding available ports...";
//     if (!bind_available(listen_fd, listen_port)) {
//         std::cerr << "No available ports found. Exiting.\n";
//         return 1;
//     }

//     std::cout << "Found port " << listen_port << "\n";
//     std::cout << "Listening at port " << listen_port << "\n";

//     pthread_t listener;
//     pthread_create(&listener, nullptr, listener_thread, &listen_fd);

//     while(1){
//         print_menu();
//         int choice = 0;

//         if (!(std::cin >> choice)) {
//             std::cout << "Invalid choice. Enter a valid number.\n";
//             std::cin.clear();
//             std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
//             continue;
//         }
//         else if (choice == 1) {
//             std::cout << "\nSearching for files... done.\n";
//             bool files_found = false;

//             for (int p : ports) {
//                 auto files = request_files(p);

//                 for (const std::string &f : files) {
//                     size_t bracket_pos = f.find("] ");
//                     if (bracket_pos != std::string::npos && f[0] == '[') {
//                         int key = std::stoi(f.substr(1, bracket_pos - 1));
//                         std::string filename = f.substr(bracket_pos + 2);

//                         if (available_files.find(key) == available_files.end()) {
//                             if (!files_found) {
//                                 std::cout << "Files available.\n";
//                                 files_found = true;
//                             }

//                             available_files[key] = std::make_pair(p, filename);
//                             std::cout << f << "\n";
//                         }
//                         else {
//                             files_found = true;
//                             std::cout << f << "\n";
//                         }
//                     }
//                 }
//             }

//             if (!files_found) {
//                 std::cout << "No files available.\n";
//             }
//         }
//         else if (choice == 2) {
//             int file_id;
//             std::cout << "\nEnter file ID: ";
//             std::cin >> file_id;

//             int *file_id_ptr = new int(file_id);
//             pthread_t download_thread;

//             if (available_files.find(file_id) == available_files.end()){
//                 std::cout << "File not found.\n";
//             }
//             else {
//                 std::cout << "Download started. File: [" << file_id << "] " << available_files[file_id].second << "\n";
//                 pthread_create(&download_thread, nullptr, download_file, file_id_ptr);
//                 pthread_detach(download_thread);
//             }
//         }

//         else if (choice == 3) {
//             //download status
//         }
//         else if (choice == 4) {
//             std::cout << "Exiting...\n";
//             // free pointers and close everything
//             break;
//         }
//         else {
//             std::cout << "Invalid choice. Enter a valid number.\n";
//         }
//     }

//     return 0;
// }