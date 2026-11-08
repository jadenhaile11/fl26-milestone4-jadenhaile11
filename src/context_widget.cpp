#include "aiws/gui/context_widget.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSlider>
#include <QString>
#include <QVBoxLayout>

namespace aiws::gui {

ContextWidget::ContextWidget(QWidget* parent) : QWidget(parent) {
    // Creates the Context section.
    auto* main_layout = new QVBoxLayout(this);
    auto* group = new QGroupBox("Context", this);
    auto* group_layout = new QVBoxLayout(group);

    // Creates the token budget slider and value label.
    auto* budget_layout = new QHBoxLayout();

    context_budget_slider_ = new QSlider(Qt::Horizontal, group);
    context_budget_slider_->setRange(50, 1000);
    context_budget_slider_->setSingleStep(50);
    context_budget_slider_->setPageStep(50);
    context_budget_slider_->setTickInterval(50);
    context_budget_slider_->setValue(300);

    budget_value_label_ = new QLabel("300", group);

    budget_layout->addWidget(new QLabel("Token Budget:", group));
    budget_layout->addWidget(context_budget_slider_, 1);
    budget_layout->addWidget(budget_value_label_);
    group_layout->addLayout(budget_layout);    

    // Creates the Build Context button and output display.
    build_context_button_ = new QPushButton("Build Context", group);
    group_layout->addWidget(build_context_button_);

    context_text_ = new QPlainTextEdit(group);
    context_text_->setReadOnly(true);
    group_layout->addWidget(context_text_);
    main_layout->addWidget(group);

    // Connects the slider and button to their local handlers.
    connect(context_budget_slider_, &QSlider::valueChanged, this, &ContextWidget::budget_changed);
    connect(build_context_button_, &QPushButton::clicked, this, &ContextWidget::request_context);

    set_build_context_enabled(false);
}

int ContextWidget::token_budget() const {
    return context_budget_slider_->value();
}

void ContextWidget::set_build_context_enabled(bool enabled) {
    build_context_button_->setEnabled(enabled);
}

// Displays each context item returned by ProcessingCore.
void ContextWidget::set_context(const std::vector<ContextItem>& items) {
    QString output;

    for (const ContextItem& item : items) {
        output += "Document: " + QString::fromStdString(item.document_id) + "\n";
        output += "Chunk: " + QString::number(
            static_cast<qulonglong>(item.chunk_sequence)) + "\n";
        output += "Tokens: " + QString::number(
            static_cast<qulonglong>(item.token_count)) + "\n";
        output += "Score: " + QString::number(item.score) + "\n";
        output += "Truncated: " + QString(item.truncated ? "Yes" : "No") + "\n";
        output += "Text: " + QString::fromStdString(item.text) + "\n\n";
    }

    context_text_->setPlainText(output);
}

void ContextWidget::clear_context() {
    context_text_->clear();
}

// Restores the Context section to its startup state.
void ContextWidget::reset() {
    context_budget_slider_->setValue(300);
    budget_value_label_->setText("300");
    clear_context();
    set_build_context_enabled(false);
}

// Sends the context request to MainWindow.
void ContextWidget::request_context() {
    if (!build_context_button_->isEnabled()) {
        return;
    }

    emit build_context_requested(token_budget());
}

// Updates the displayed value when the slider moves.
void ContextWidget::budget_changed(int value) {
    budget_value_label_->setText(QString::number(value));
}

}  // namespace aiws::gui
