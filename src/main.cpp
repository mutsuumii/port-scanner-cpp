#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include <cstring>

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <sys/socket.h>
  #include <arpa/inet.h>
  #include <netdb.h>
  #include <unistd.h>
  #include <fcntl.h>
  #include <poll.h>
#endif

struct ScanResult {
    int port;
    bool open;
};

std::mutex print_mutex;
std::vector<int> open_ports;
std::mutex results_mutex;

bool scan_port(const std::string& target, int port, int timeout_ms) {
    int sock;
#ifdef _WIN32
    sock = static_cast<int>(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (sock == INVALID_SOCKET) return false;

    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
#else
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return false;

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif

    struct sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (inet_pton(AF_INET, target.c_str(), &addr.sin_addr) <= 0) {
        struct addrinfo hints{}, *res;
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(target.c_str(), nullptr, &hints, &res) != 0) {
#ifdef _WIN32
            closesocket(sock);
#else
            close(sock);
#endif
            return false;
        }
        addr.sin_addr = reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr;
        freeaddrinfo(res);
    }

    connect(sock, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr));

#ifdef _WIN32
    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);
    struct timeval tv;
    tv.tv_sec = timeout_ms / 1000;
    tv.tv_usec = (timeout_ms % 1000) * 1000;
    bool connected = select(sock + 1, nullptr, &write_fds, nullptr, &tv) > 0;
    closesocket(sock);
#else
    struct pollfd pfd{};
    pfd.fd = sock;
    pfd.events = POLLOUT;
    bool connected = poll(&pfd, 1, timeout_ms) > 0 && (pfd.revents & POLLOUT);
    close(sock);
#endif

    return connected;
}

void scan_range(const std::string& target, int start, int end,
                int timeout_ms, int num_threads) {
    std::atomic<int> next_port(start);
    std::vector<std::thread> threads;

    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([&]() {
            while (true) {
                int port = next_port.fetch_add(1);
                if (port > end) break;
                if (scan_port(target, port, timeout_ms)) {
                    {
                        std::lock_guard<std::mutex> lock(print_mutex);
                        std::cout << "  [+] " << port << "/tcp OPEN\n";
                    }
                    {
                        std::lock_guard<std::mutex> lock(results_mutex);
                        open_ports.push_back(port);
                    }
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "Usage: port_scanner <target> [start=1] [end=1024] [threads=200] [timeout_ms=1000]\n";
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
#endif

    std::string target = argv[1];
    int start = argc > 2 ? std::stoi(argv[2]) : 1;
    int end = argc > 3 ? std::stoi(argv[3]) : 1024;
    int threads = argc > 4 ? std::stoi(argv[4]) : 200;
    int timeout_ms = argc > 5 ? std::stoi(argv[5]) : 1000;

    std::cout << "[*] Scanning " << target << " ports " << start << "-" << end << "\n";
    std::cout << "[*] Threads: " << threads << ", Timeout: " << timeout_ms << "ms\n";

    auto t0 = std::chrono::high_resolution_clock::now();
    scan_range(target, start, end, timeout_ms, threads);
    auto t1 = std::chrono::high_resolution_clock::now();

    double elapsed = std::chrono::duration<double>(t1 - t0).count();

    std::sort(open_ports.begin(), open_ports.end());
    std::cout << "\n[*] Scan complete. " << open_ports.size() << " open port(s) found in "
              << elapsed << "s\n";

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
