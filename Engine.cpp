#include "Engine.h"

#include <algorithm>

ICell::Value DefaultCell::GetValue() const {
    if (std::holds_alternative<std::string>(value)
            && std::get<std::string>(value).front() == kEscapeSign) {
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
                cols.resize(pos.col);
            }
        }
    }
    if (auto cell = GetCell(pos); (cell && cell->GetText() == text))
        return;

    if (auto cell = cells.at(pos.row).at(pos.col); !cell.expired()) {
        dep_graph.InvalidOutcoming(cell.lock());
        dep_graph.Delete(cell.lock());
    }

    if (text.size() > 1 && text.front() == kFormulaSign){
        auto val = std::make_shared<FormulaCell>(ICell::Value(text), this);
        cells[pos.row][pos.col] = dep_graph.AddVertex(val);
    } else {
        auto val = std::make_shared<DefaultCell>(ICell::Value(text));
        cells[pos.row][pos.col] = dep_graph.AddVertex(val);
    }
}

const ICell *SpreadSheet::GetCell(Position pos) const {
    if (!(pos < size) || cells.at(pos.row).at(pos.col).expired())
        return nullptr;

    return cells.at(pos.row).at(pos.col).lock().get();
}

ICell *SpreadSheet::GetCell(Position pos) {
    if (!(pos < size) || cells.at(pos.row).at(pos.col).expired())
        return nullptr;

    return cells.at(pos.row).at(pos.col).lock().get();
}

// TODO если ячейка была самой граничной, то размер стоит уменьшить!
void SpreadSheet::ClearCell(Position pos) {
    if (!(size < pos))
        return;

    if (auto cell = cells.at(pos.row).at(pos.col); !cell.expired()) {
        dep_graph.InvalidOutcoming(cell.lock());
        dep_graph.Delete(cell.lock());
    }
}

Size SpreadSheet::GetPrintableSize() const {
    return size;
}

DependencyGraph &SpreadSheet::GetGraph() {
    return dep_graph;
}