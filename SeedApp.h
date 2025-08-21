#ifndef SEEDAPP_H
#define SEEDAPP_H

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

class SeedApp
{
public:
  static const int num_ports = 5;
  static const int buffer_size = 1024;
  static const int ports[num_ports];
  static int listen_fd, listen_port;
  static std::string directory_path;
  static std::map<int, std::string> available_files;
  static std::mutex files_mutex;

  static void print_menu();
  static int run();
  static int count_sources(int file_id, const std::string &filename);
};

#endif