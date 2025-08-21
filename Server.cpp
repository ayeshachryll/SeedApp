#include "Server.h"
#include "SeedApp.h"

std::map<int, std::string> Server::list_files()
{
  std::map<int, std::string> map_files;
  DIR *dir = opendir(SeedApp::directory_path.c_str());

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

    std::string full_path = SeedApp::directory_path + "/" + name;

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

void *Server::handle_client_thread(void *arg)
{
  int client_fd = *(int *)arg;
  delete (int *)arg;

  char buffer[SeedApp::buffer_size];
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
    std::string file_path = SeedApp::directory_path + "/" + file_id;

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

void *Server::listener_thread(void *arg)
{
  struct sockaddr_in addr{};
  socklen_t addrlen = sizeof(addr);

  while (1)
  {
    int new_socket = accept(SeedApp::listen_fd, (struct sockaddr *)&addr, &addrlen);
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

bool Server::bind_available(int &out_fd, int &out_port)
{
  int opt = 1;
  struct sockaddr_in addr{};

  for (int i = 0; i < SeedApp::num_ports; ++i)
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
    addr.sin_port = htons(SeedApp::ports[i]);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0)
    {
      SeedApp::listen_fd = fd;
      SeedApp::listen_port = SeedApp::ports[i];

      if (listen(SeedApp::listen_fd, 4) < 0)
      {
        perror("Listen failed");
        close(SeedApp::listen_fd);
        exit(EXIT_FAILURE);
      }

      return true;
    }

    close(fd);
  }
  return false;
}