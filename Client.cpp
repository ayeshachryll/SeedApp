#include "Client.h"
#include "SeedApp.h"
#include <sys/stat.h>

void *Client::download_file(void *arg)
{
  int file_id = *(int *)arg;
  delete (int *)arg;

  if (SeedApp::available_files.find(file_id) == SeedApp::available_files.end())
  {
    perror("File ID not on list.");
    return nullptr;
  }

  std::string filename = SeedApp::available_files[file_id];

  for (int target_port : SeedApp::ports)
  {
    if (target_port == SeedApp::listen_port)
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

    bool has_file = false;
    if (connect(sock, (sockaddr *)&addr, sizeof(addr)) == 0)
    {
      send(sock, "LIST", 4, 0);
      char buffer[SeedApp::buffer_size];
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

void *Client::request_files(void *arg)
{
  int port = *(int *)arg;
  delete (int *)arg;

  if (port == SeedApp::listen_port)
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
    char buffer[SeedApp::buffer_size];
    ssize_t n = recv(sock, buffer, sizeof(buffer), 0);
    if (n > 0)
    {
      buffer[n] = '\0';
      std::string data(buffer);
      size_t pos = 0;

      {
        std::lock_guard<std::mutex> lock(SeedApp::files_mutex);

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

            SeedApp::available_files[key] = filename;
          }
        }
      }
    }
  }
  close(sock);
  return nullptr;
}