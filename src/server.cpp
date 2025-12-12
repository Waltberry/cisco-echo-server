#include "common.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

constexpr int BACKLOG = 128;
constexpr int READ_CHUNK = 4096;

struct ClientState {
    std::string inbuf;   // bytes received but not yet processed into full lines
    std::string outbuf;  // bytes queued to send (handles partial writes)
};

int make_listen_socket(uint16_t port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        std::cerr << "socket() failed: " << errno_string() << "\n";
        return -1;
    }

    int yes = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
        std::cerr << "setsockopt(SO_REUSEADDR) failed: " << errno_string() << "\n";
        ::close(fd);
        return -1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "bind() failed: " << errno_string() << "\n";
        ::close(fd);
        return -1;
    }

    if (::listen(fd, BACKLOG) < 0) {
        std::cerr << "listen() failed: " << errno_string() << "\n";
        ::close(fd);
        return -1;
    }

    return fd;
}

bool set_poll_events_for_client(pollfd &pfd, const ClientState &st) {
    // Always listen for input
    short ev = POLLIN;

    // If we have data to send, also listen for POLLOUT
    if (!st.outbuf.empty()) {
        ev |= POLLOUT;
    }
    pfd.events = ev;
    return true;
}

void remove_client(
    int client_fd,
    std::vector<pollfd> &pfds,
    std::unordered_map<int, ClientState> &clients
) {
    ::close(client_fd);
    clients.erase(client_fd);

    // Remove from pfds vector
    for (size_t i = 0; i < pfds.size(); ++i) {
        if (pfds[i].fd == client_fd) {
            pfds.erase(pfds.begin() + static_cast<long>(i));
            break;
        }
    }
}

} // namespace

int main(int argc, char **argv) {
    uint16_t port = 9090;
    if (argc >= 2) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    int listen_fd = make_listen_socket(port);
    if (listen_fd < 0) return 1;

    std::cout << "Echo server listening on 0.0.0.0:" << port << "\n";
    std::cout << "Protocol: send lines ending with \\n; server echoes each line.\n";

    std::vector<pollfd> pfds;
    pfds.push_back(pollfd{listen_fd, POLLIN, 0});

    std::unordered_map<int, ClientState> clients;

    while (true) {
        // Refresh poll events for clients depending on whether they have pending output
        for (size_t i = 1; i < pfds.size(); ++i) {
            int fd = pfds[i].fd;
            auto it = clients.find(fd);
            if (it != clients.end()) {
                set_poll_events_for_client(pfds[i], it->second);
            }
        }

        int n = ::poll(pfds.data(), static_cast<nfds_t>(pfds.size()), -1);
        if (n < 0) {
            if (errno == EINTR) continue; // interrupted by signal
            std::cerr << "poll() failed: " << errno_string() << "\n";
            break;
        }

        // 1) Accept new connections
        if (pfds[0].revents & POLLIN) {
            sockaddr_in caddr{};
            socklen_t clen = sizeof(caddr);
            int cfd = ::accept(listen_fd, reinterpret_cast<sockaddr*>(&caddr), &clen);
            if (cfd < 0) {
                std::cerr << "accept() failed: " << errno_string() << "\n";
            } else {
                char ipbuf[INET_ADDRSTRLEN]{};
                ::inet_ntop(AF_INET, &caddr.sin_addr, ipbuf, sizeof(ipbuf));
                uint16_t cport = ntohs(caddr.sin_port);
                std::cout << "Client connected fd=" << cfd << " from " << ipbuf << ":" << cport << "\n";

                pfds.push_back(pollfd{cfd, POLLIN, 0});
                clients.emplace(cfd, ClientState{});
            }
        }

        // 2) Handle client events
        // We may remove clients while iterating, so iterate carefully using index.
        for (size_t i = 1; i < pfds.size(); /* increment inside */) {
            int cfd = pfds[i].fd;
            short re = pfds[i].revents;

            // If hung up or error: remove
            if (re & (POLLHUP | POLLERR | POLLNVAL)) {
                std::cout << "Client disconnected fd=" << cfd << " (HUP/ERR)\n";
                remove_client(cfd, pfds, clients);
                continue; // do not increment i; pfds shifted
            }

            auto it = clients.find(cfd);
            if (it == clients.end()) {
                // Should not happen, but be defensive
                ++i;
                continue;
            }
            ClientState &st = it->second;

            // Read incoming data
            if (re & POLLIN) {
                char buf[READ_CHUNK];
                ssize_t r = ::recv(cfd, buf, sizeof(buf), 0);
                if (r == 0) {
                    std::cout << "Client disconnected fd=" << cfd << "\n";
                    remove_client(cfd, pfds, clients);
                    continue;
                }
                if (r < 0) {
                    std::cerr << "recv() failed fd=" << cfd << ": " << errno_string() << "\n";
                    remove_client(cfd, pfds, clients);
                    continue;
                }

                st.inbuf.append(buf, static_cast<size_t>(r));

                // Extract complete lines ending with '\n'
                while (true) {
                    size_t pos = st.inbuf.find('\n');
                    if (pos == std::string::npos) break;

                    // Include the newline in echoed output
                    std::string line = st.inbuf.substr(0, pos + 1);
                    st.inbuf.erase(0, pos + 1);

                    // Echo it back (queue for sending)
                    st.outbuf += line;
                }
            }

            // Write pending output (handle partial writes)
            if ((re & POLLOUT) && !st.outbuf.empty()) {
                ssize_t s = ::send(cfd, st.outbuf.data(), st.outbuf.size(), 0);
                if (s < 0) {
                    std::cerr << "send() failed fd=" << cfd << ": " << errno_string() << "\n";
                    remove_client(cfd, pfds, clients);
                    continue;
                }
                st.outbuf.erase(0, static_cast<size_t>(s));
            }

            ++i;
        }
    }

    ::close(listen_fd);
    return 0;
}
