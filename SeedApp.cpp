#include "SeedApp.h"
#include "Server.h"
#include "Client.h"
#include <limits>

const int SeedApp::ports[SeedApp::num_ports] = {8999, 9000, 9002, 9003, 9004};
int SeedApp::listen_fd;
int SeedApp::listen_port;
std::string SeedApp::directory_path = "./files";
std::map<int, std::string> SeedApp::available_files;
std::mutex SeedApp::files_mutex;

void SeedApp::print_menu()
{
  std::cout << "\nSeed App\n";
  std::cout << "[1] List available files.\n";
  std::cout << "[2] Download file.\n";
  std::cout << "[3] Download status.\n";
  std::cout << "[4] Exit.\n";
  std::cout << "\n? ";
}

int SeedApp::count_sources(int file_id, const std::string &filename)
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

int SeedApp::run()
{
  std::cout << "Finding available ports...";
  if (!Server::bind_available(listen_fd, listen_port))
  {
    std::cerr << "No available ports found. Exiting.\n";
    return 1;
  }

  std::cout << "Found port " << listen_port << "\n";
  std::cout << "Listening at port " << listen_port << "\n";

  pthread_t listener;
  pthread_create(&listener, nullptr, Server::listener_thread, &listen_fd);

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
          pthread_create(&thread, nullptr, Client::request_files, port_ptr);
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

        std::string filename = available_files[file_id];
        int sources_count = count_sources(file_id, filename);

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
          pthread_create(&download_thread, nullptr, Client::download_file, file_id_ptr);
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

int main()
{
  return SeedApp::run();
}