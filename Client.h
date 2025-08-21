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

class Client
{
public:
  static void *download_file(void *arg);
  static void *request_files(void *arg);
  static void *download_status(void *arg);
};

#endif