#include "common.hpp"

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

int connect_tcp(const std::string &host, const std::string &port) {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo *res = nullptr;
    int rc = ::getaddrinfo(host.c_str(), port.c_str(), &hints, &res);
    if (rc != 0) {
        std::cerr << "getaddrinfo() failed: " << gai_strerror(rc) << "\n";
        return -1;
    }

    int fd = -1;
    for (addrinfo *p = res; p != nullptr; p = p->ai_next) {
        fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (fd < 0) continue;

        if (::connect(fd, p->ai_addr, p->ai_addrlen) == 0) {
            break; // connected
        }

        ::close(fd);
        fd = -1;
    }

    ::freeaddrinfo(res);
    return fd;
}

bool send_all(int fd, const char *data, size_t len) {
    size_t off = 0;
    while (off < len) {
        ssize_t n = ::send(fd, data + off, len - off, 0);
        if (n < 0) return false;
        off += static_cast<size_t>(n);
    }
    return true;
}

} // namespace

int main(int argc, char **argv) {
    std::string host = "127.0.0.1";
    std::string port = "9090";

    if (argc >= 2) host = argv[1];
    if (argc >= 3) port = argv[2];

    int fd = connect_tcp(host, port);
    if (fd < 0) {
        std::cerr << "Failed to connect to " << host << ":" << port << "\n";
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << "\n";
    std::cout << "Type lines and press Enter. Ctrl-D to quit.\n";

    std::string line;
    while (std::getline(std::cin, line)) {
        line.push_back('\n');

        if (!send_all(fd, line.data(), line.size())) {
            std::cerr << "send() failed: " << errno_string() << "\n";
            ::close(fd);
            return 1;
        }

        // Read back exactly one line (until '\n')
        std::string echoed;
        char c;
        while (true) {
            ssize_t r = ::recv(fd, &c, 1, 0);
            if (r == 0) {
                std::cerr << "Server closed connection.\n";
                ::close(fd);
                return 0;
            }
            if (r < 0) {
                std::cerr << "recv() failed: " << errno_string() << "\n";
                ::close(fd);
                return 1;
            }
            echoed.push_back(c);
            if (c == '\n') break;
        }

        std::cout << "echo: " << echoed;
    }

    ::close(fd);
    std::cout << "\nDisconnected.\n";
    return 0;
}
