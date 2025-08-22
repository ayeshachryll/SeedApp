#ifndef SERVER_H
#define SERVER_H

#include <iostream>
#include <string>
#include <map>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

class server
{
public:
    static std::map<int, std::pair<std::string, long>> list_files();
    static void *handle_client_thread(void *arg);
    static void *accept_thread(void *arg);
    static bool bind_available(int &out_fd, int &out_port);
};

#endif