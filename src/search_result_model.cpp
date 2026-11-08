#include "aiws/gui/search_result_model.hpp"

#include <QString>
#include <utility>

namespace aiws::gui {

SearchResultModel::SearchResultModel(QObject* parent)
    : QAbstractTableModel(parent) {
}

// Returns the number of search results.
int SearchResultModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return static_cast<int>(results_.size());
}

// The results table contains five columns.
int SearchResultModel::columnCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }

    return 5;
}

// Provides the data displayed in each table cell.
QVariant SearchResultModel::data(const QModelIndex& index, int role) const {
       if (!index.isValid() || role != Qt::DisplayRole) {
            return {};
    }

    const SearchResult* result = result_at(index.row());
    if (result == nullptr) {
        return {};
    }

    switch (index.column()) {
        case 0:
            return index.row() + 1;
        case 1:
            return QString::fromStdString(result->document_id);
        case 2:
            return static_cast<qulonglong>(result->chunk_sequence);
        case 3:
            return result->score;
        case 4:
            return QString::fromStdString(result->text);
        default:
            return {};
    } 
}

// Sets the column headers for the table.
QVariant SearchResultModel::headerData(int section,
                                       Qt::Orientation orientation,
                                       int role) const {
    if (role != Qt::DisplayRole) {
        return {};
    }

    if (orientation == Qt::Horizontal) {
        switch (section) {
            case 0:
                return QString("Rank");
            case 1:
                return QString("Document");
            case 2:
                return QString("Chunk");
            case 3:
                return QString("Score");
            case 4:
                return QString("Text");
            default:
                return {};
        }
    }

    return QAbstractTableModel::headerData(section, orientation, role);
}

// Replaces the stored results and notifies the table.
void SearchResultModel::set_results(std::vector<SearchResult> results) {
    beginResetModel();
    results_ = std::move(results);
    endResetModel();
}

// Clears all stored search results.
void SearchResultModel::clear() {
    beginResetModel();
    results_.clear();
    endResetModel();
}

// Returns a result pointer only when the row is valid.
const SearchResult* SearchResultModel::result_at(int row) const noexcept {
    if (row < 0 || static_cast<std::size_t>(row) >= results_.size()) {
        return nullptr;
    }
    
    return &results_[static_cast<std::size_t>(row)];
}

}  // namespace aiws::gui
