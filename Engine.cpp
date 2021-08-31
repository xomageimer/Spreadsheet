#include "Engine.h"

#include <algorithm>

ICell::Value DefaultCell::GetValue() const {
    if (std::holds_alternative<std::string>(value)
            && std::get<std::string>(value).front() == '\'') {
            return ICell::Value(std::get<std::string>(value).substr(1));
    }
    return value;
}

std::string DefaultCell::GetText() const {
    return std::get<std::string>(value);
}

std::vector<Position> DefaultCell::GetReferencedCells() const {
    return std::vector<Position>{};
}



FormulaCell::FormulaCell(ICell::Value text_val, ISheet * sheet) : ICell(std::move(text_val)), sheet_(sheet) {}

ICell::Value FormulaCell::GetValue() const {
    if (status == Status::Invalid) evaluated_value = Evaluate(*sheet_);
    if (std::holds_alternative<double>(evaluated_value))
        return ICell::Value(std::get<double>(evaluated_value));
    else
        return ICell::Value(std::get<FormulaError>(evaluated_value));
}

std::string FormulaCell::GetText() const {
    if (std::holds_alternative<FormulaError>(value)){
        return std::string{std::get<FormulaError>(value).ToString()};
    } else {
        return std::get<std::string>(value);
    }
}

std::vector<Position> FormulaCell::GetReferencedCells() const {
    return pos_of_used_cells;
}

std::string FormulaCell::GetExpression() const {
    // TODO строить выражение из AST
    return "";
}

IFormula::Value FormulaCell::Evaluate(const ISheet &sheet) const { // TODO не забыть сделать здесь статус = VALID
    return IFormula::Value();
}

// TODO мб обработку исключения TableTooBigException надо вынести отдельно, дабы избежать копипасты
void SpreadSheet::SetCell(Position pos, std::string text) {
    if (size < pos){
        if (pos.row > size.rows) {
            if (size.rows == Position::kMaxRows)
                throw TableTooBigException("The number of rows is greater/equal than the maximum");

            size.rows = pos.row;
            cells.resize(pos.row);
        }
        if (pos.col > size.cols) {
            if (size.cols == Position::kMaxCols)
                throw TableTooBigException("The number of cols is greater/equal than the maximum");

            size.cols = pos.col;
            for (auto &cols : cells) {
                cols.resize(pos.col, nullptr);
            }
        }
    }

    dep_graph.InvalidOutcoming(pos);
    if (text.size() > 1 && text.front() == '='){
        auto val = cells[pos.row][pos.col] = std::make_shared<FormulaCell>(ICell::Value(text), this);
    } else {
        cells[pos.row][pos.col] = std::make_shared<DefaultCell>(ICell::Value(text));
    }
}

const ICell *SpreadSheet::GetCell(Position pos) const {
    if (!(pos < size))
        return nullptr;
    return cells.at(pos.row).at(pos.col).get();
}

ICell *SpreadSheet::GetCell(Position pos) {
    if (!(pos < size))
        return nullptr;
    return cells.at(pos.row).at(pos.col).get();
}

// TODO если ячейка была самой граничной, то размер стоит уменьшить!
void SpreadSheet::ClearCell(Position pos) {
    if (!(size < pos))
        return;

    dep_graph.InvalidOutcoming(pos);
    cells[pos.row][pos.col] = nullptr;
}

void SpreadSheet::InsertRows(int before, int count) {
    if (size.rows + count >= Position::kMaxRows)
        throw TableTooBigException("The number of rows is greater/equal than the maximum");

    auto it = cells.begin() + before;
    cells.insert(it, count, std::vector<std::shared_ptr<ICell>>{});
    for (int i = 0; i < count; i++) {
        it->resize(size.cols, nullptr);
    }

    size.rows += count;

    for (int i = before + count; i < size.rows; i++) {
        for (int j = 0; j < size.cols; j++) {
            dep_graph.InvalidOutcoming({i - count, j});
            dep_graph.Reset({i - count, j}, {i, j});
            if (cells[i][j] != nullptr) {
                auto formula = dynamic_cast<FormulaCell *>(cells[i][j].get());
                if (formula) {
                    formula->HandleInsertedRows(before, count);
                }
            }
        }
    }
}

void SpreadSheet::InsertCols(int before, int count) {
    if (size.cols + count >= Position::kMaxCols)
        throw TableTooBigException("The number of cols is greater/equal than the maximum");

    for (auto & row : cells) {
        auto it = row.begin() + before;
        it = row.insert(it, count, nullptr);
    }

    size.cols += count;

    for (int i = 0; i < size.rows; i++) {
        for (int j = before + count; j < size.cols; j++) {
            dep_graph.InvalidOutcoming({i, j - count});
            dep_graph.Reset({i, j - count}, {i, j});
            if (cells[i][j] != nullptr) {
                auto formula = dynamic_cast<FormulaCell *>(cells[i][j].get());
                if (formula) {
                    formula->HandleInsertedCols(before, count);
                }
            }
        }
    }
}

Size SpreadSheet::GetPrintableSize() const {
    return size;
}

DependencyGraph &SpreadSheet::GetGraph() {
    return dep_graph;
}

void SpreadSheet::DeleteRows(int first, int count) {
    for (int i = first; i < size.rows; i++) {
        for (int j = 0; j < size.cols; j++) {
            dep_graph.InvalidOutcoming({i, j});
            if (i < first + count)
                dep_graph.Delete({i, j});
            else
                dep_graph.Reset({i, j}, {i, j - count});

            if (cells[i][j] != nullptr) {
                auto formula = dynamic_cast<FormulaCell *>(cells[i][j].get());
                if (formula) {
                    formula->HandleDeletedCols(first, count);
                }
            }
        }
    }

    auto it = cells.begin() + first;
    cells.erase(it, it + count);
    size.rows = (size.rows >= count) ? size.rows - count : 0;
}

void SpreadSheet::DeleteCols(int first, int count) {
    for (int i = 0; i < size.rows; i++) {
        for (int j = first; j < size.cols; j++) {
            dep_graph.InvalidOutcoming({i, j});
            if (j < first + count)
                dep_graph.Delete({i, j});
            else
                dep_graph.Reset({i, j}, {i - count, j});

            if (cells[i][j] != nullptr) {
                auto formula = dynamic_cast<FormulaCell *>(cells[i][j].get());
                if (formula) {
                    formula->HandleDeletedRows(first, count);
                }
            }
        }
    }

    for (auto & row : cells) {
        auto it = row.begin() + first;
        row.erase(it, it + count);
    }
    size.cols = (size.cols >= count) ? size.cols - count : 0;
}