#include "aiws/gui/main_window.hpp"
#include "aiws/gui/ipc_server.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <iostream>
#include <QMessageBox>
#include <cstdlib>
#include <string>
#include <unistd.h>

// Asks the user to confirm before exiting.
void confirm_exit(QWidget* parent) {
    QMessageBox::StandardButton answer = QMessageBox::question(parent,
                                                               "Confirm Exit",
                                                               "Are you sure you want to exit?",
                                                               QMessageBox::Yes | QMessageBox::No,
                                                               QMessageBox::No);
    if (answer == QMessageBox::Yes) {
        QApplication::quit();
    }
}

// Uses the same default socket path as aiws_send.
std::string default_socket_path() {
    const char* user = std::getenv("USER");

    if (user != nullptr && *user != '\0') {
        return "/tmp/aiws-" + std::string(user) + ".sock";
    }

    return "/tmp/aiws-" + std::to_string(getuid()) + ".sock";
} 

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Handles the M4 build-delay and CR-1 socket options.
    QCommandLineParser parser;
    parser.setApplicationDescription("M4 Responsive AI Workspace");
    parser.addHelpOption();
    
    QCommandLineOption delay_option("build-delay-ms", "Extra build time per document in milliseconds.", "milliseconds", "0");

    QCommandLineOption socket_option("ipc-socket", "Unix socket path for remote document requests.", "path");

    parser.addOption(delay_option);
    parser.addOption(socket_option);
    parser.process(app);

    bool valid = false;
    int delay = parser.value(delay_option).toInt(&valid);

    if (!valid || delay < 0) {
        std::cerr << "Invalid build delay. \n";
        return 1;
    }

    std::string socket_path = default_socket_path();

    if (parser.isSet(socket_option)) {
        socket_path = parser.value(socket_option).toStdString();
    }

    aiws::gui::MainWindow window;
    window.set_build_delay_ms(delay);
    window.set_exit_callback(&confirm_exit);

    // Starts CR-1 IPC server on its own background thread.
    aiws::gui::IpcServer server(window);

    if (!server.start(socket_path)) {
        std::cerr << "Could not start IPC server at " << socket_path << '\n';
        return 1;
    }
    window.resize(900, 900);
    window.show();

    int result = app.exec();

    // Stops serving and removes the socket before exiting.
    server.stop();

    return result;
}