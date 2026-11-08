#include "aiws/gui/term_inspector_widget.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

namespace aiws::gui {

TermInspectorWidget::TermInspectorWidget(QWidget* parent) : QWidget(parent) {
    // Creates the Term Inspector section.
    auto* main_layout = new QVBoxLayout(this);
    auto* group = new QGroupBox("Term Inspector", this);
    auto* group_layout = new QVBoxLayout(group);
    auto* input_layout = new QHBoxLayout();

    term_edit_ = new QLineEdit(group);
    term_edit_->setPlaceholderText("Enter term");

    inspect_button_ = new QPushButton("Inspect", group);

    input_layout->addWidget(new QLabel("Term:", group));
    input_layout->addWidget(term_edit_, 1);
    input_layout->addWidget(inspect_button_);

    group_layout->addLayout(input_layout);

    // Displays document frequency and selected-chunk frequency.
    auto* df_layout = new QHBoxLayout();
    auto* tf_layout = new QHBoxLayout();

    document_frequency_value_ = new QLabel("-", group);
    chunk_frequency_value_ = new QLabel("-", group);

    df_layout->addWidget(new QLabel("Document Frequency (DF):", group));
    df_layout->addWidget(document_frequency_value_);
    df_layout->addStretch();
    tf_layout->addWidget(new QLabel("Selected Chunk TF:", group));
    tf_layout->addWidget(chunk_frequency_value_);
    tf_layout->addStretch();

    group_layout->addLayout(df_layout);
    group_layout->addLayout(tf_layout);

    main_layout->addWidget(group);

    // Connects the Inspect button to its local handler.
    connect(inspect_button_, &QPushButton::clicked, this, &TermInspectorWidget::request_inspection);

    set_inspection_enabled(false);
}

void TermInspectorWidget::set_inspection_enabled(bool enabled) {
    inspect_button_->setEnabled(enabled);
}

// Stores the chunk associated with the selected search result.
void TermInspectorWidget::set_selected_chunk(const QString& chunk_id) {
    selected_chunk_id_ = chunk_id;
}

// Displays statistics returned by ProcessingCore.
void TermInspectorWidget::set_statistics(std::size_t document_frequency,
                                         bool has_chunk_frequency,
                                         std::size_t chunk_frequency) {
    
    document_frequency_value_->setText(QString::number(static_cast<qulonglong>(document_frequency)));

    if (has_chunk_frequency) {
        chunk_frequency_value_->setText(QString::number(static_cast<qulonglong>(chunk_frequency)));
    } else {
        chunk_frequency_value_->setText("-");
    }
}

// Clears previously displayed statistics.
void TermInspectorWidget::clear_statistics() {
    document_frequency_value_->setText("-");
    chunk_frequency_value_->setText("-");
}

void TermInspectorWidget::reset() {
    term_edit_->clear();
    selected_chunk_id_.clear();
    clear_statistics();
    set_inspection_enabled(false);
}

void TermInspectorWidget::request_inspection() {
    const QString term = term_edit_->text().trimmed();

    if (!inspect_button_->isEnabled() || term.isEmpty()) {
        return;
    }

    emit term_inspection_requested(term, selected_chunk_id_);
}

}  // namespace aiws::gui
