#include "aiws/gui/main_window.hpp"

#include <QApplication>
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

    aiws::gui::MainWindow window;

    window.set_exit_callback(&confirm_exit);
    window.resize(900, 900);
    window.show();

    return app.exec();
}