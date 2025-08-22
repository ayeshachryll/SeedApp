#ifndef SEED_H
#define SEED_H

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

class seed
{
public:
    static const int num_ports = 5;
    static const int buffer_size = 1024;
    static const int ports[num_ports];
    static int listen_fd, listen_port;
    static std::string directory_path;
    static std::map<int, std::pair<std::string, long>> available_files;
    static std::map<int, std::string> current_downloads;
    static long bytes_downloaded;
    static long file_size;
    static std::mutex files_mutex;

    static void print_menu();
    static int run();
    static int count_sources(int file_id, const std::string &filename);
};

#endif