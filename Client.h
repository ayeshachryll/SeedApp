#ifndef CLIENT_H
#define CLIENT_H

#include <iostream>
#include <string>
#include <map>
#include <vector>
#include <mutex>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>

class client
{
public:
    static bool show_status_display;
    static void *download_file(void *arg);
    static void *request_files(void *arg);
    static void *download_status(void *arg);
    static void show_download_progress();
    static void hide_download_progress();
};

#endif