#include "aiws/gui/workspace_widget.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

namespace aiws::gui {

WorkspaceWidget::WorkspaceWidget(QWidget* parent) : QWidget(parent) {
    // Creates the Workspace section.
    auto* main_layout = new QHBoxLayout(this);

    auto* workspace_group = new QGroupBox("Workspace", this);
    auto* workspace_layout = new QVBoxLayout(workspace_group);

    document_list_ = new QListWidget(workspace_group);
    workspace_layout->addWidget(document_list_);

    auto* button_layout = new QHBoxLayout();
    add_document_button_ = new QPushButton("Add Document", workspace_group);
    build_corpus_button_ = new QPushButton("Build Corpus", workspace_group);

    button_layout->addWidget(add_document_button_);
    button_layout->addWidget(build_corpus_button_);
    workspace_layout->addLayout(button_layout);

    corpus_status_label_ = new QLabel("Corpus: Not built", workspace_group);
    workspace_layout->addWidget(corpus_status_label_);
    
    // Creates the Selected Document section.
    auto* selected_group = new QGroupBox("Selected Document", this);
    auto* selected_layout = new QVBoxLayout(selected_group);

    document_title_label_ = new QLabel(selected_group);
    selected_layout->addWidget(document_title_label_);

    document_text_ = new QPlainTextEdit(selected_group);
    document_text_->setReadOnly(true);
    selected_layout->addWidget(document_text_);

    main_layout->addWidget(workspace_group, 1);
    main_layout->addWidget(selected_group, 1);

    // Connect user actions to the widget's signals.
    connect(add_document_button_, &QPushButton::clicked, this, &WorkspaceWidget::add_document_requested);
    connect(build_corpus_button_, &QPushButton::clicked, this, &WorkspaceWidget::build_corpus_requested);
    connect(document_list_, &QListWidget::currentRowChanged, this, &WorkspaceWidget::document_selected);

    set_build_corpus_enabled(false);
}

void WorkspaceWidget::add_document(const QString& title) {
    document_list_->addItem(title);
}

void WorkspaceWidget::select_document(int row) {
    if (row >= 0 && row < document_list_->count()) {
        document_list_->setCurrentRow(row);
    }
}

void WorkspaceWidget::show_selected_document(const QString& title,
                                              const QString& text) {
    document_title_label_->setText(title);
    document_text_->setPlainText(text);
}

void WorkspaceWidget::clear_selected_document() {
    document_title_label_->clear();
    document_text_->clear();
}

void WorkspaceWidget::set_corpus_status(const QString& text) {
    corpus_status_label_->setText(text);
}

void WorkspaceWidget::set_build_corpus_enabled(bool enabled) {
    build_corpus_button_->setEnabled(enabled);
}

// Restores the workspace to its original startup state.
void WorkspaceWidget::reset() {
    document_list_->clear();
    clear_selected_document();
    set_corpus_status("Corpus: Not built");
    set_build_corpus_enabled(false);
}

}  // namespace aiws::gui