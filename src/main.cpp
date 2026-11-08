#include "aiws/gui/main_window.hpp"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <iostream>
#include <QMessageBox>

// Ask the user to confirm before exiting.
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

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // Handles the M4 build-delay command-line option.
    QCommandLineParser parser;
    parser.setApplicationDescription("M4 Responsive AI Workspace");
    parser.addHelpOption();
    
    QCommandLineOption delay_option("build-delay-ms", "Extra build time per document in milliseconds.", "milliseconds", "0");

    parser.addOption(delay_option);
    parser.process(app);

    bool valid = false;
    int delay = parser.value(delay_option).toInt(&valid);

    if (!valid || delay < 0) {
        std::cerr << "Invalid build delay. \n";
        return 1;
    }

    aiws::gui::MainWindow window;

    window.set_build_delay_ms(delay);
    window.set_exit_callback(&confirm_exit);
    window.resize(900, 900);
    window.show();

    return app.exec();
}