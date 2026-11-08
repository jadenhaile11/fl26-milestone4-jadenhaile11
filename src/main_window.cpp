#include "aiws/gui/main_window.hpp"
#include "aiws/gui/context_widget.hpp"
#include "aiws/gui/resettable.hpp"
#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/term_inspector_widget.hpp"
#include "aiws/gui/workspace_widget.hpp"

#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>

#include <algorithm>
#include <chrono>
#include <future>
#include <exception>
#include <utility>
#include <vector>

namespace aiws::gui {

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // TODO: Construct the M3 application window, compose the GUI components,
    // establish layouts/presentation, connect events, and establish initial state.
    setWindowTitle("AI Workspace");
    resize(900, 900);

    // Sets up main window and its vertical layout.
    auto* central = new QWidget(this);
    auto* main_layout = new QVBoxLayout(central);
    main_layout->setSpacing(8);
    main_layout->setContentsMargins(12, 12, 12, 12);

    auto* header = new QLabel("AI Workspace\nDocument Search and Content Explorer", central);
    header->setAlignment(Qt::AlignCenter);
    header->setStyleSheet(
        "QLabel {background-color: #66232e; color: white; padding: 12px;"
        "font-size: 19px; font-weight: bold;}");
    main_layout->addWidget(header);

    // Creates the four main sections of the GUI.
    workspace_widget_ = new WorkspaceWidget(central);
    search_widget_ = new SearchWidget(central);
    term_inspector_widget_ = new TermInspectorWidget(central);
    context_widget_ = new ContextWidget(central);

    main_layout->addWidget(workspace_widget_, 3);
    main_layout->addWidget(search_widget_, 3);
    
    auto* bottom_layout = new QHBoxLayout();
    bottom_layout->addWidget(term_inspector_widget_, 1);
    bottom_layout->addWidget(context_widget_, 1);
    main_layout->addLayout(bottom_layout, 2);

    auto* footer = new QHBoxLayout();
    status_label_ = new QLabel("Ready", central);
    reset_button_ = new QPushButton("Reset", central);
    exit_button_ = new QPushButton("Exit", central);
    reset_button_->setStyleSheet(
        "QPushButton {background-color: #d9a441; color: black;"
        "padding: 7px 18px; font-weight: bold; }");
    exit_button_->setStyleSheet(
        "QPushButton {background-color: #3469a3; color: white;"
        "padding: 7px 18px; font-weight: bold;}");
    footer->addWidget(status_label_, 1);
    footer->addWidget(reset_button_);
    footer->addWidget(exit_button_);
    main_layout->addLayout(footer);
    setCentralWidget(central);

    // Connects user actions to the MainWindow handlers.
    connect(workspace_widget_, &WorkspaceWidget::add_document_requested, this, &MainWindow::add_document);
    connect(workspace_widget_, &WorkspaceWidget::build_corpus_requested, this, &MainWindow::build_corpus);
    connect(workspace_widget_, &WorkspaceWidget::document_selected, this, &MainWindow::document_selected);
    connect(search_widget_, &SearchWidget::search_requested, this, &MainWindow::search_requested);
    connect(search_widget_, &SearchWidget::query_changed, this, &MainWindow::query_changed);
    connect(search_widget_, &SearchWidget::source_document_requested, this, &MainWindow::source_document_requested);
    connect(search_widget_, &SearchWidget::selected_chunk_changed, term_inspector_widget_, &TermInspectorWidget::set_selected_chunk);
    connect(term_inspector_widget_, &TermInspectorWidget::term_inspection_requested, this, &MainWindow::term_inspection_requested);
    connect(context_widget_, &ContextWidget::build_context_requested, this, &MainWindow::build_context_requested);
    connect(reset_button_, &QPushButton::clicked, this, &MainWindow::reset_application);
    connect(exit_button_, &QPushButton::clicked, this, &MainWindow::exit_application);

    // Poll completion without blocking Qt's event loop.
    build_timer_ = new QTimer(this);
    build_timer_->setInterval(25);
    connect(build_timer_, &QTimer::timeout, this, &MainWindow::check_build_completion);

    update_action_states();
}

MainWindow::~MainWindow() {
    build_timer_->stop();
    if (build_thread_.joinable()) build_thread_.join();
}

void MainWindow::set_exit_callback(ExitCallback callback) noexcept {
    exit_callback_ = callback;
}

// The dialog and IPC server can both use this file-add operation.
// A rejected file must not change the workspace or the corpus state.
bool MainWindow::add_document_file(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;

    const QByteArray bytes = file.readAll();
    if (file.error() != QFileDevice::NoError) return false;

    const QFileInfo info(path);
    const QString content = QString::fromUtf8(bytes);
    workspace_.add_document(Document(info.absoluteFilePath().toStdString(), info.fileName().toStdString(), content.toStdString()));
    workspace_widget_->add_document(info.fileName());
    workspace_revision_++;
    invalidate_corpus();
    status_label_->setText("Document added: " + info.fileName());
    return true;
}

void MainWindow::set_build_delay_ms(int ms_per_document) {
    build_delay_ms_ = std::max(0, ms_per_document);
}

// Loads a selected text file and invalidates any previously built corpus.
void MainWindow::add_document() {
    const QString path = QFileDialog::getOpenFileName(this, "Add Document", QString(), "Text Files (*.txt);;All Files (*)");
    if (path.isEmpty()) return;
    if (!add_document_file(path)) {
        QMessageBox::warning(this, "File Error", "Could not open the selected file.");
    }
}

// Copies the workspace on the GUI thread; only the copy goes to the worker.
void MainWindow::build_corpus() {
    if (building_ || workspace_.documents().empty()) return;

    Workspace snapshot = workspace_;
    build_revision_ = workspace_revision_;
    const int delay = build_delay_ms_;
    std::promise<ProcessingCore> promise;
    build_future_ = promise.get_future();

    // Any completed previous worker must be joined before the thread object
    // is assigned a new thread. No worker touches GUI widgets or workspace_.
    if (build_thread_.joinable()) build_thread_.join();
    build_thread_ = std::thread(
        [snapshot = std::move(snapshot), delay, promise = std::move(promise)]() mutable {
            try {
                // Simulates expensive per document work inside the worker.
                for (std::size_t i = 0; i < snapshot.documents().size(); ++i) {
                    if (delay > 0)
                        std::this_thread::sleep_for(std::chrono::milliseconds(delay));
                }
                ProcessingCore result;
                result.rebuild(snapshot);
                promise.set_value(std::move(result));
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        });

    building_ = true;
    invalidate_corpus();
    status_label_->setText("Building corpus...");
    workspace_widget_->set_corpus_status("Corpus: Building");
    update_action_states();
    build_timer_->start();
}

void MainWindow::check_build_completion() {
    if (!building_ || !build_future_.valid() ||
        build_future_.wait_for(std::chrono::milliseconds(0)) !=
            std::future_status::ready) return;

    build_timer_->stop();
    if (build_thread_.joinable()) build_thread_.join();

    try {
        ProcessingCore result = build_future_.get();
        building_ = false;
#ifndef AIWS_FAULT_ACCEPT_STALE
        if (build_revision_ != workspace_revision_) {
            invalidate_corpus();
            status_label_->setText("Build discarded: Workspace changed.");
            return;
        }
#endif
        processing_core_ = std::move(result);
        corpus_current_ = true;
        clear_processing_outputs();
        workspace_widget_->set_corpus_status(
            QString("Corpus: %1 chunks").arg(processing_core_.chunk_count()));
        status_label_->setText("Corpus built.");
        update_action_states();
    } catch (const std::exception& e) {
#ifdef AIWS_FAULT_LOSE_FAILURE
        // Fault injection: failure is not reported and state stays Building.
        return;
#else
        building_ = false;
        invalidate_corpus();
        status_label_->setText("Build failed: " + QString::fromUtf8(e.what()));
#endif
    }
}

void MainWindow::document_selected(int row) {
    const auto& docs = workspace_.documents();
    if (row < 0 || static_cast<std::size_t>(row) >= docs.size()) {
        workspace_widget_->clear_selected_document();
        return;
    }
    
    const auto& doc = docs[static_cast<std::size_t>(row)];
    workspace_widget_->show_selected_document(QString::fromStdString(doc.title()), QString::fromStdString(doc.text()));
}

// Runs search only when the corpus is current and the query is valid.
void MainWindow::search_requested(const QString& query, int k) {
    if (!corpus_current_ || query.trimmed().isEmpty()) return;

    try {
        auto results = processing_core_.search(query.toStdString(), k);
        search_widget_->set_results(std::move(results));
        term_inspector_widget_->set_selected_chunk(QString());
        term_inspector_widget_->clear_statistics();
        context_widget_->clear_context();
        status_label_->setText("Search completed.");
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Search Error", QString::fromUtf8(e.what()));
    }
}

void MainWindow::query_changed(const QString& query) {
    (void)query;
    update_action_states();
}

void MainWindow::build_context_requested(int token_budget) {
    if (!corpus_current_ || !search_widget_->has_query()) return;

    try {
        auto items = processing_core_.build_context(
            search_widget_->query().toStdString(), search_widget_->result_count(),
            static_cast<std::size_t>(token_budget));
        context_widget_->set_context(items);
        status_label_->setText("Context built.");
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Context Error", QString::fromUtf8(e.what()));
    }    
}

// Finds and displays the original document for a selected search result.
void MainWindow::source_document_requested(const QString& document_id) {
    const auto& docs = workspace_.documents();
    for (std::size_t i = 0; i < docs.size(); ++i) {
        if (QString::fromStdString(docs[i].id()) == document_id) {
            workspace_widget_->select_document(static_cast<int>(i));
            document_selected(static_cast<int>(i));
            status_label_->setText("Source document selected.");
            return;
        }
    }
}

void MainWindow::term_inspection_requested(const QString& term,
                                           const QString& chunk_id) {
    if (!corpus_current_ || term.trimmed().isEmpty()) return;
    
    try {
        const auto df = processing_core_.document_frequency(term.toStdString());
        const bool has_chunk = !chunk_id.isEmpty();
        const auto tf = has_chunk ? processing_core_.term_frequency(term.toStdString(), chunk_id.toStdString()) : 0;
        term_inspector_widget_->set_statistics(df, has_chunk, tf);
        status_label_->setText("Term inspected.");
    } catch (const std::exception& e) {
        QMessageBox::warning(this, "Inspection Error", QString::fromUtf8(e.what()));
    }
}

// Resets all GUI components through their shared resettable interface.
void MainWindow::reset_application() {
    // Invalidates any background build started before Reset.
    workspace_revision_++;

    workspace_.clear();
    corpus_current_ = false;

    std::vector<Resettable*> components = {workspace_widget_, search_widget_, term_inspector_widget_, context_widget_};

    for (Resettable* component : components) {
        component->reset();
    }

    update_action_states();
    status_label_->setText("Application reset.");
}

void MainWindow::exit_application() {
    if (exit_callback_ != nullptr) exit_callback_(this);
}

// Enables processing buttons only when their prerequisites are met.
void MainWindow::update_action_states() {
    workspace_widget_->set_build_corpus_enabled(!workspace_.documents().empty() && !building_);
    const bool can_process = corpus_current_ && search_widget_->has_query();
    search_widget_->set_search_enabled(can_process);
    context_widget_->set_build_context_enabled(can_process);
    term_inspector_widget_->set_inspection_enabled(corpus_current_);
}

// A workspace change makes old search results and context invalid.
void MainWindow::invalidate_corpus() {
    corpus_current_ = false;
    workspace_widget_->set_corpus_status("Corpus: Not built");
    clear_processing_outputs();
    update_action_states();
}

void MainWindow::clear_processing_outputs() {
    search_widget_->clear_results();
    term_inspector_widget_->set_selected_chunk(QString());
    term_inspector_widget_->clear_statistics();
    context_widget_->clear_context();
}

}  // namespace aiws::gui