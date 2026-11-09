#include "aiws/gui/ipc_server.hpp"
#include "aiws/gui/main_window.hpp"

#include <QMetaObject>
#include <QString>

#include <arpa/inet.h>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>


namespace aiws::gui {

namespace {

// Maximum size of a document path message.
constexpr std::uint32_t max_path_size = 1024 * 1024;

// Reads an exact number of bytes, including split messages.
bool read_exact(int fd, void* buffer, std::size_t count, const std::atomic<bool>& stopping) {
    auto* data = static_cast<unsigned char*>(buffer);
    std::size_t received = 0;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

    while (received < count && !stopping.load()) {
        if (std::chrono::steady_clock::now() >= deadline)
            return false;

        pollfd pfd{fd, POLLIN, 0};
        int ready = poll(&pfd, 1, 100);

        if (ready < 0) {
            if (errno == EINTR) continue;
            return false;
        }

        if (ready == 0) continue;

        ssize_t n = recv(fd, data + received, count - received, 0);

        if (n <= 0) {
            if (n < 0 && errno == EINTR) continue;
            return false;
        }

        received += static_cast<std::size_t>(n);
    }

    return received == count && !stopping.load();
}

// Checks readability without involving the GUI thread.
bool readable_file(const std::string& path) {
    if (path.empty() || path.find('\0') != std::string::npos)
        return false;

    int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) return false;

    struct stat info {};
    bool valid = fstat(fd, &info) == 0 && S_ISREG(info.st_mode);

    if (valid) {
        char byte;
        ssize_t n = read(fd, &byte, 1);
        valid = n >= 0;
    }

    close(fd);
    return valid;
}

} // namespace

IpcServer::IpcServer(MainWindow& window) : window_(window) {}

IpcServer::~IpcServer() {
    stop();
}

// Creates a Unix domain socket and starts the server thread.
bool IpcServer::start(const std::string& socket_path) {
    if (worker_.joinable() || listen_fd_ != -1)
        return false;

    if (socket_path.empty() ||
        socket_path.size() >= sizeof(sockaddr_un::sun_path))
        return false;

    socket_path_ = socket_path;
    stopping_ = false;
    listen_fd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (listen_fd_ < 0) return false;

    sockaddr_un address {};
    address.sun_family = AF_UNIX;
    std::memcpy(address.sun_path, socket_path.c_str(), socket_path.size() + 1);

    // Does not remove an existing socket belonging to another app.
    if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close(listen_fd_);
        listen_fd_ = -1;
        return false;
    }

    if (listen(listen_fd_, 8) != 0) {
        close(listen_fd_);
        listen_fd_ = -1;
        unlink(socket_path_.c_str());
        return false;
    }

    worker_ = std::thread(&IpcServer::serve, this);
    return true;
}

// Joins the worker and removes the socket file.
void IpcServer::stop() {
    stopping_ = true;

    if (worker_.joinable())
        worker_.join();

    if (listen_fd_ != -1) {
        close(listen_fd_);
        listen_fd_ = -1;
        unlink(socket_path_.c_str());
    }
}

// Handles socket communication on a separate std::thread.
void IpcServer::serve() {
    while (!stopping_.load()) {
        pollfd listener{listen_fd_, POLLIN, 0};

        int ready = poll(&listener, 1, 100);
        if (ready < 0) {
            if (errno == EINTR) continue;
            break;
        }

        if (ready == 0) continue;
        if (!(listener.revents & POLLIN)) continue;

        int client = accept(listen_fd_, nullptr, nullptr);
        if (client < 0) continue;

        // Protocol: four-byte network-order length followed by that many UTF-8 path bytes.
        std::uint32_t network_length = 0;

        bool valid = read_exact(client, &network_length, sizeof(network_length), stopping_);

        std::string path;

        if (valid) {
            std::uint32_t length = ntohl(network_length);

            valid = length > 0 && length <= max_path_size;

            if (valid) {
                path.resize(length);
                valid = read_exact(client, path.data(), length, stopping_);
            }
        }

        valid = valid && readable_file(path);


        // Queue document additions on the GUI thread.
        // The socket thread never directly modifies Qt widgets.
        if (valid && !stopping_.load()) {
            valid = QMetaObject::invokeMethod(
                &window_,
                [window = &window_,
                 file_path = QString::fromUtf8(
                     path.data(),
                     static_cast<qsizetype>(path.size()))]() {
                    window->add_document_file(file_path);
                },
                Qt::QueuedConnection);
        }

        // Replies without waiting for the GUI to process the request.
        if (!stopping_.load()) {
            const unsigned char reply = valid ? 0 : 1;
            send(client, &reply, 1, MSG_NOSIGNAL);
        }

        // Closes the client's connection before accepting another.
        close(client);
    }
}

} // namespace aiws::gui