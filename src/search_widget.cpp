#include "aiws/gui/search_widget.hpp"
#include "aiws/gui/search_result_model.hpp"

#include <QAbstractItemView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QSpinBox>
#include <QTableView>

#include <utility>


namespace aiws::gui {

SearchWidget::SearchWidget(QWidget* parent) : QWidget(parent) {
    // Creates the main Query/Search section.
    auto* main_layout = new QVBoxLayout(this);
    auto* query_group = new QGroupBox("Query / Search", this);
    auto* query_layout = new QVBoxLayout(query_group);
    auto* controls_layout = new QHBoxLayout();

    query_edit_ = new QLineEdit(query_group);
    query_edit_->setPlaceholderText("Enter search query");

    result_count_spin_ = new QSpinBox(query_group);
    result_count_spin_->setRange(1, 100);
    result_count_spin_->setValue(3);

    search_button_ = new QPushButton("Search", query_group);

    controls_layout->addWidget(new QLabel("Query:", query_group));
    controls_layout->addWidget(query_edit_, 1);
    controls_layout->addWidget(new QLabel("Results (k):", query_group));
    controls_layout->addWidget(result_count_spin_);
    controls_layout->addWidget(search_button_);

    query_layout->addLayout(controls_layout);

    // Displays ranked search results using QT Model/View
    results_view_ = new QTableView(query_group);
    result_model_ = new SearchResultModel(this);

    results_view_->setModel(result_model_);
    results_view_->setSelectionBehavior(QAbstractItemView::SelectRows);
    results_view_->setSelectionMode(QAbstractItemView::SingleSelection);
    results_view_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    results_view_->horizontalHeader()->setStretchLastSection(true);

    query_layout->addWidget(results_view_);

    // Displays details of the selected search result.
    auto* selected_layout = new QHBoxLayout();

    selected_result_text_ = new QPlainTextEdit(query_group);
    selected_result_text_->setReadOnly(true);

    view_source_button_ = new QPushButton("View Source", query_group);
    view_source_button_->setEnabled(false);

    selected_layout->addWidget(new QLabel("Selected Result:", query_group));
    selected_layout->addWidget(selected_result_text_, 1);
    selected_layout->addWidget(view_source_button_);

    query_layout->addLayout(selected_layout);
    main_layout->addWidget(query_group);

    // Connects local events to their handlers.
    connect(search_button_, &QPushButton::clicked, this, &SearchWidget::request_search);
    connect(query_edit_, &QLineEdit::textChanged, this, &SearchWidget::query_changed);
    connect(results_view_->selectionModel(), &QItemSelectionModel::selectionChanged, this, &SearchWidget::show_selected_result);
    connect(view_source_button_, &QPushButton::clicked, this, &SearchWidget::request_source_document);

    set_search_enabled(false);
}

QString SearchWidget::query() const {
    return query_edit_->text();
}

int SearchWidget::result_count() const {
    return result_count_spin_->value();
}

bool SearchWidget::has_query() const {
    return !query().trimmed().isEmpty();
}

void SearchWidget::set_search_enabled(bool enabled) {
    search_button_->setEnabled(enabled);
}

// Replaces the table results and clears previous selection.
void SearchWidget::set_results(std::vector<SearchResult> results) {
    result_model_->set_results(std::move(results));
    results_view_->clearSelection();
    selected_result_text_->clear();
    view_source_button_->setEnabled(false);

    emit selected_chunk_changed(QString());
}

void SearchWidget::clear_results() {
    results_view_->clearSelection();
    result_model_->clear();
    selected_result_text_->clear();
    view_source_button_->setEnabled(false);

    emit selected_chunk_changed(QString());
}

// Restores the original Query/Search state.
void SearchWidget::reset() {
    query_edit_->clear();
    result_count_spin_->setValue(3);
    clear_results();
    set_search_enabled(false); 
}

void SearchWidget::request_search() {
    if (!search_button_->isEnabled() || !has_query()) {
        return;
    }

    emit search_requested(query(), result_count());
}

// Updates the selected result without running another search.
void SearchWidget::show_selected_result() {
    const QModelIndex index = results_view_->currentIndex();
    const SearchResult* result = result_model_->result_at(index.row());

    if (!index.isValid() || result == nullptr) {
        selected_result_text_->clear();
        view_source_button_->setEnabled(false);
        emit selected_chunk_changed(QString());
        return;
    }

    QString details =
        "Document: " + QString::fromStdString(result->document_id) +
        "\nChunk: " + QString::number(result->chunk_sequence) +
        "\nScore: " + QString::number(result->score) +
        "\nMatched Terms: " + QString::number(result->matched_terms) +
        "\n\nText:\n" + QString::fromStdString(result->text);

    selected_result_text_->setPlainText(details);
    view_source_button_->setEnabled(true);

    emit selected_chunk_changed(QString::fromStdString(result->chunk_id));
}

// Requests the original document for the selected result.
void SearchWidget::request_source_document() {
    const QModelIndex index = results_view_->currentIndex();
    const SearchResult* result = result_model_->result_at(index.row());

    if (!index.isValid() || result == nullptr) {
        return;
    }

    emit source_document_requested(QString::fromStdString(result->document_id));
}

}  // namespace aiws::gui