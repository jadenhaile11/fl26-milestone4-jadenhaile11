#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <string>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace {

using Clock = std::chrono::steady_clock;

// Uses the same default socket path as the AI Workspace.
std::string default_socket_path() {
    const char* user = std::getenv("USER");

    if (user != nullptr && *user != '\0') {
        return "/tmp/aiws-" + std::string(user) + ".sock";
    }

    return "/tmp/aiws-" + std::to_string(getuid()) + ".sock";
}

// Waits until the socket is ready without exceeding the deadline.
bool wait_for_socket(int fd, short events, Clock::time_point deadline) {
    while (Clock::now() < deadline) {
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now());

        int timeout = static_cast<int>(remaining.count());
        if (timeout < 1) timeout = 1;

        pollfd pfd{fd, events, 0};
        int result = poll(&pfd, 1, timeout);

        if (result > 0) {
            return (pfd.revents & events) != 0;
        }

        if (result < 0 && errno != EINTR) {
            return false;
        }
    }

    return false;
}

// Connects to the application's Unix socket within two seconds.
int connect_to_server(const std::string& socket_path) {
    if (socket_path.empty() || socket_path.size() >= sizeof(sockaddr_un::sun_path)) {
        return -1;
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) return -1;

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 ||
        fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        close(fd);
        return -1;
    }

    sockaddr_un address{};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socket_path.c_str(), socket_path.size() + 1);

    int result = connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address));

    if (result < 0 && errno != EINPROGRESS) {
        close(fd);
        return -1;
    }

    if (result < 0) {
        auto deadline = Clock::now() + std::chrono::seconds(2);

        if (!wait_for_socket(fd, POLLOUT, deadline)) {
            close(fd);
            return -1;
        }

        int error = 0;
        socklen_t length = sizeof(error);

        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0 || error != 0) {
            close(fd);
            return -1;
        }
    }

    return fd;
}

// Sends every byte, even when a write sends only part of the message.
bool send_all(int fd, const void* buffer, std::size_t count, Clock::time_point deadline) {
    const auto* data = static_cast<const unsigned char*>(buffer);
    std::size_t sent = 0;

    while (sent < count) {
        if (!wait_for_socket(fd, POLLOUT, deadline)) return false;

        ssize_t n = send(fd, data + sent, count - sent, MSG_NOSIGNAL);

        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }

        if (n <= 0) return false;

        sent += static_cast<std::size_t>(n);
    }

    return true;
}

// Reads the one-byte response from the server.
bool receive_reply(int fd, unsigned char& reply, Clock::time_point deadline) {
    while (Clock::now() < deadline) {
        if (!wait_for_socket(fd, POLLIN, deadline))
            return false;

        ssize_t n = recv(fd, &reply, 1, 0);

        if (n == 1) return true;

        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
            continue;
        }

        return false;
    }

    return false;
}

} // namespace

int main(int argc, char* argv[]) {
    std::string socket_path = default_socket_path();
    std::string file_path;

    // Supported commands: aiws_send add FILE and aiws_send --socket PATH add FILE
    int index = 1;

    if (index < argc && std::string(argv[index]) == "--socket") {
        if (index + 1 >= argc) {
            std::cerr << "Missing socket path.\n";
            return 1;
        }

        socket_path = argv[index + 1];
        index += 2;
    }

    if (argc - index != 2 ||
        std::string(argv[index]) != "add") {
        std::cerr << "Usage: aiws_send [--socket PATH] add FILE\n";
        return 1;
    }

    try {
        // Relative paths refer to the sender's working directory.
        file_path = std::filesystem::absolute(std::filesystem::path(argv[index + 1])).string();
    } catch (const std::exception& e) {
        std::cerr << "Invalid file path: " << e.what() << '\n';
        return 1;
    }

    // The server accepts paths up to 1 MiB
    if (file_path.empty() || file_path.size() > 1024 * 1024) {
        std::cerr << "File path is too long.\n";
        return 1;
    }

    int fd = connect_to_server(socket_path);

    if (fd < 0) {
        std::cerr << "Could not connect to AI Workspace.\n";
        return 2;
    }

    // Protocol: four-byte big-endian path length followed by the raw UTF-8 path bytes.
    std::uint32_t length = htonl(static_cast<std::uint32_t>(file_path.size()));

    auto deadline = Clock::now() + std::chrono::seconds(5);
    bool success = send_all(fd, &length, sizeof(length), deadline) && send_all(fd, file_path.data(), file_path.size(), deadline);
    unsigned char reply = 0;

    if (success) {
        success = receive_reply(fd, reply, deadline);
    }

    close(fd);

    if (!success) {
        std::cerr << "Communication with AI Workspace failed.\n";
        return 2;
    }

    if (reply == 1) {
        std::cerr << "Document request rejected: file is unreadable or invalid.\n";
        return 1;
    }

    if (reply != 0) {
        std::cerr << "Invalid server response.\n";
        return 2;
    }

    std::cout << "Document request accepted.\n";
    return 0;
}
