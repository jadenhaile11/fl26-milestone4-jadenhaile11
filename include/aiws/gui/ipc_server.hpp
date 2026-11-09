#pragma once

#include <atomic>
#include <string>
#include <thread>

namespace aiws::gui {

class MainWindow;

class IpcServer final {
public:
    explicit IpcServer(MainWindow& window);

    // Stops the server and joins its worker thread.
    ~IpcServer();

    // Prevents copying a server that owns a thread.
    IpcServer(const IpcServer&) = delete;
    IpcServer& operator=(const IpcServer&) = delete;

    // Starts listening at the specified socket path.
    bool start(const std::string& socket_path);

    // Stops listening and cleans up the socket.
    void stop();

private:
    // Receives incoming requests on the worker thread.
    void serve();

    MainWindow& window_;
    std::string socket_path_;

    std::thread worker_;
    std::atomic<bool> stopping_{false};

    int listen_fd_{-1};
};

} // namespace aiws::gui