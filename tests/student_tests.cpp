
#include "aiws/gui/main_window.hpp"
#include "aiws/gui/workspace_widget.hpp"

#include <QFile>
#include <QLabel>
#include <QListWidget>
#include <QMetaObject>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTest>

using aiws::gui::MainWindow;
using aiws::gui::WorkspaceWidget;

class StudentTests : public QObject {
    Q_OBJECT

private:
    // Creates a temporary document for a test.
    QString create_file(const QString& directory, const QString& name) {
        const QString path = directory + "/" + name;

        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            return QString();
        }

        file.write("Virginia Tech document testing corpus.");
        file.close();
        return path;
    }

    // Finds the corpus status displayed in the workspace.
    QString corpus_status(MainWindow& window) {
        auto* workspace = window.findChild<WorkspaceWidget*>();
        if (!workspace) return QString();

        for (QLabel* label : workspace->findChildren<QLabel*>()) {
            if (label->text().startsWith("Corpus:")) {
                return label->text();
            }
        }

        return QString();
    }

    // Finds the status message at the bottom of MainWindow.
    QString application_status(MainWindow& window) {
        for (QLabel* label : window.findChildren<QLabel*>()) {
            if (label->text().startsWith("Build ") || label->text() == "Building corpus...") {return label->text();}
        }

        return QString();
    }

    // Starts a build using MainWindow's existing Qt slot.
    bool start_build(MainWindow& window) {
        return QMetaObject::invokeMethod(&window, "build_corpus", Qt::DirectConnection);
    }

private slots:
    // A readable file should be added successfully.
    void add_valid_document() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = create_file(directory.path(), "valid.txt");
        QVERIFY(!path.isEmpty());

        MainWindow window;

        QVERIFY(window.add_document_file(path));

        auto* list = window.findChild<QListWidget*>();
        QVERIFY(list != nullptr);
        QCOMPARE(list->count(), 1);
        QCOMPARE(corpus_status(window), QString("Corpus: Not built"));
    }

    // An unreadable file must not change the workspace.
    void reject_invalid_document() {
        MainWindow window;

        QVERIFY(!window.add_document_file(
            "/tmp/aiws-this-file-does-not-exist-12345.txt"));

        auto* list = window.findChild<QListWidget*>();
        QVERIFY(list != nullptr);
        QCOMPARE(list->count(), 0);
        QCOMPARE(corpus_status(window), QString("Corpus: Not built"));
    }

    // Slow corpus build must not block the GUI thread
    void background_build_is_responsive() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path =
            create_file(directory.path(), "background.txt");
        QVERIFY(!path.isEmpty());

        MainWindow window;
        QVERIFY(window.add_document_file(path));

        window.set_build_delay_ms(400);

        QVERIFY(start_build(window));
        QCOMPARE(corpus_status(window), QString("Corpus: Building"));

        // Qt can still process GUI events while building.
        QTest::qWait(30);
        QCOMPARE(corpus_status(window), QString("Corpus: Building"));

        QTRY_VERIFY_WITH_TIMEOUT(corpus_status(window).contains("chunks"), 3000);
    }

    // A workspace modification makes an in-progress build stale
    void reject_stale_build() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString first = create_file(directory.path(), "first.txt");
        const QString second = create_file(directory.path(), "second.txt");

        QVERIFY(!first.isEmpty());
        QVERIFY(!second.isEmpty());

        MainWindow window;

        QVERIFY(window.add_document_file(first));
        window.set_build_delay_ms(400);

        QVERIFY(start_build(window));

        // Change the workspace before the build completes.
        QVERIFY(window.add_document_file(second));

        QTRY_VERIFY_WITH_TIMEOUT(application_status(window).contains("discarded"), 3000);
        QVERIFY(!corpus_status(window).contains("chunks"));
    }

    // Resets need to invalidate a build started before Reset
    void reset_rejects_old_build() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = create_file(directory.path(), "reset.txt");
        QVERIFY(!path.isEmpty());

        MainWindow window;
        QVERIFY(window.add_document_file(path));

        window.set_build_delay_ms(400);
        QVERIFY(start_build(window));

        // Clicks the same Reset button used by the application.
        QPushButton* reset = nullptr;

        for (QPushButton* button :
             window.findChildren<QPushButton*>()) {
            if (button->text() == "Reset") {
                reset = button;
                break;
            }
        }

        QVERIFY(reset != nullptr);
        reset->click();

        QTRY_VERIFY_WITH_TIMEOUT(application_status(window).contains("discarded"), 3000);
        QVERIFY(!corpus_status(window).contains("chunks"));
    }

    // A failed build must leave the Building state.
    void build_failure_is_reported() {
        QTemporaryDir directory;
        QVERIFY(directory.isValid());

        const QString path = create_file(directory.path(), "duplicate.txt");
        QVERIFY(!path.isEmpty());
        MainWindow window;

        // The backend rejects duplicate document IDs.
        QVERIFY(window.add_document_file(path));
        QVERIFY(window.add_document_file(path));

        window.set_build_delay_ms(50);
        QVERIFY(start_build(window));

        QTRY_VERIFY_WITH_TIMEOUT(application_status(window).startsWith("Build failed:"), 3000);
        QCOMPARE(corpus_status(window), QString("Corpus: Not built"));
    }
};

QTEST_MAIN(StudentTests)

#include "student_tests.moc"