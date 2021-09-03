#include "Engine.h"

#include <algorithm>


ICell::Value DefaultCell::GetValue() const {
    if (formula_) {
        auto eval_val = formula_->GetValue();
        ICell::Value val;
        if (std::holds_alternative<double>(eval_val))
            return std::get<double>(eval_val);
        else
            return std::get<FormulaError>(eval_val);
    }
    else if (std::holds_alternative<std::string>(value)
            && std::get<std::string>(value).front() == kEscapeSign) {
            return ICell::Value(std::get<std::string>(value).substr(1));
    }
    return value;
}

std::string DefaultCell::GetText() const {
    return std::get<std::string>(value);
}

std::vector<Position> DefaultCell::GetReferencedCells() const {
    if (formula_)
        return formula_->GetReferencedCells();
    return std::vector<Position>{};
}

DefaultCell::DefaultCell(const std::string &text, ISheet const & sheet) : ICell(ICell::Value(text)) {
    if (text.size() > 1 && text.front() == '='){
        formula_ = std::make_shared<DefaultFormula>(text.substr(1), &sheet);
    }
}



DefaultFormula::DefaultFormula(std::string const & val, const ISheet * sheet) : sheet_(sheet) {
    BuildAST(val);
}

std::unique_ptr<IFormula> ParseFormula(const std::string& expression) {
    auto formula = std::make_unique<DefaultFormula>(expression);
    return formula;
}

IFormula::Value DefaultFormula::GetValue() const {
    if (status == Status::Invalid) evaluated_value = Evaluate(*sheet_);
    if (std::holds_alternative<double>(evaluated_value))
        return IFormula::Value(std::get<double>(evaluated_value));
    else
        return IFormula::Value(std::get<FormulaError>(evaluated_value));
}

std::vector<Position> DefaultFormula::GetReferencedCells() const {
    return as_tree->GetCells();
}

std::string DefaultFormula::GetExpression() const {
    return as_tree->GetExpression();
}

IFormula::Value DefaultFormula::Evaluate(const ISheet &sheet) const {
    if (status == Status::Error)
        return evaluated_value;

    IFormula::Value val;
    try {
        val = as_tree->Evaluate();
        status = Status::Valid;
    } catch (FormulaError & fe) {
        status = Status::Error;

        evaluated_value = fe;
    }
    return val;
}

IFormula::HandlingResult DefaultFormula::HandleInsertedRows(int before, int count) {
    if (GetReferencedCells().empty())
        return IFormula::HandlingResult::NothingChanged;
    return as_tree->MutateRows(before, count);
}

IFormula::HandlingResult DefaultFormula::HandleInsertedCols(int before, int count) {
    if (GetReferencedCells().empty())
        return IFormula::HandlingResult::NothingChanged;
    return as_tree->MutateCols(before, count);
}

IFormula::HandlingResult DefaultFormula::HandleDeletedRows(int first, int count) {
    if (GetReferencedCells().empty())
        return IFormula::HandlingResult::NothingChanged;
    return as_tree->MutateRows(first, -count); // TODO смотреть через muteted row какое исключение вернет pos и от этого возвращать опр значение
}

IFormula::HandlingResult DefaultFormula::HandleDeletedCols(int first, int count) {
    if (GetReferencedCells().empty())
        return IFormula::HandlingResult::NothingChanged;
    return as_tree->MutateRows(first, -count);
}

void DefaultFormula::BuildAST(std::string const & text) const {
    if (!as_tree || (as_tree && as_tree->GetCells().empty())) {
        try {
            std::stringstream ss(text);
            as_tree.emplace(AST::ParseFormula(ss, *sheet_));
        } catch (FormulaError & fe) {
            status = Status::Error;

            evaluated_value = fe;
        }
    }
}



// TODO мб обработку исключения TableTooBigException надо вынести отдельно, дабы избежать копипасты
void SpreadSheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid())
        throw InvalidPositionException("invalid pos");

    if (!size || size < pos){
        if (!size.rows || pos.row >= size.rows) {
            if (size.rows == Position::kMaxRows)
                throw TableTooBigException("The number of rows is greater/equal than the maximum");

            size_t i = size.rows;
            size.rows = pos.row + 1;
            cells.resize(size.rows);
            for (int j = i; j < size.rows; j++){
                cells[j].resize(size.cols);
            }
        }
        if (!size.cols || pos.col >= size.cols) {
            if (size.cols == Position::kMaxCols)
                throw TableTooBigException("The number of cols is greater/equal than the maximum");

            size.cols = pos.col + 1;
            for (auto &cols : cells) {
                cols.resize(size.cols);
            }
        }
    }
    if (auto cell = GetCell(pos); (cell && cell->GetText() == text))
        return;

    if (auto cell = cells.at(pos.row).at(pos.col); !cell.expired()) {
        dep_graph.InvalidOutcoming(cell.lock());
        dep_graph.Delete(cell.lock());
    }

   // TODO проверять при создании корректность позиции ячейки, остальные ошибки формулы проверять при вычислениях
    auto val = std::make_shared<DefaultCell>(text, *this); // TODO если текст можно трактовать как число, то хранить в Value число
    cells[pos.row][pos.col] = dep_graph.AddVertex(val);
}

const ICell *SpreadSheet::GetCell(Position pos) const {
    if (!pos.IsValid())
        throw InvalidPositionException("invalid pos");

    if (!(pos < size) || cells.at(pos.row).at(pos.col).expired())
        return nullptr;
    return cells.at(pos.row).at(pos.col).lock().get();
}

ICell *SpreadSheet::GetCell(Position pos) {
    if (!pos.IsValid())
        throw InvalidPositionException("invalid pos");

    if (!(pos < size) || cells.at(pos.row).at(pos.col).expired())
        return nullptr;
    return cells.at(pos.row).at(pos.col).lock().get();
}

void SpreadSheet::ClearCell(Position pos) {
    if (!pos.IsValid())
        throw InvalidPositionException("invalid pos");

    if (!(size > pos))
        return;

    if (auto cell = cells.at(pos.row).at(pos.col); !cell.expired()) {
        dep_graph.InvalidOutcoming(cell.lock());
        dep_graph.Delete(cell.lock());
    }

    if (pos.col == size.cols - 1) {
        std::optional<int> max_pos;
        for (int i = pos.col; i >= 0; i--) {
            for (int j = 0; j < static_cast<int>(size.rows); j++) {
                if (!cells[j][i].expired() && (!max_pos || i > max_pos.value())) {
                    max_pos.emplace(i);
                    break;
                }
            }
        }
        for (auto & raw : cells) {
            raw.erase(raw.begin() + ((!max_pos) ? 0 : (*max_pos + 1)), raw.end());
        }
        size.cols = (!max_pos) ? 0 : (*max_pos + 1);
    }
    if (pos.row == size.rows - 1) {
        std::optional<int> max_pos;
        for (int j = size.rows - 1; j >= 0; j--){
            for (int i = 0; i < static_cast<int>(size.cols); i++){
                if (!cells[j][i].expired() && (!max_pos || j > max_pos.value())) {
                    max_pos.emplace(j);
                    break;
                }
            }
        }
        cells.erase(cells.begin() + ((!max_pos) ? 0 : (*max_pos + 1)), cells.end());
        size.rows = (!max_pos) ? 0 : (*max_pos + 1);
    }
}

void SpreadSheet::InsertRows(int before, int count) {
    if (size.rows + count > Position::kMaxRows)
        throw TableTooBigException("The number of rows is greater than the maximum");

    auto it = cells.begin() + before;
    cells.insert(it, count, std::vector<std::weak_ptr<DefaultCell>>(size.cols));
    size.rows += count;

    for (int i = before + count; i < size.rows; i++) {
        for (auto & el : cells[i]) {
            if (!el.expired()) {
                dep_graph.InvalidOutcoming(el.lock());
                auto formula = dynamic_cast<DefaultFormula *>(el.lock().get());
                if (formula) {
                    formula->HandleInsertedRows(before, count);
                }
            }
        }
    }
}

void SpreadSheet::InsertCols(int before, int count) {
    if (size.cols + count > Position::kMaxCols)
        throw TableTooBigException("The number of cols is greater than the maximum");

    int count_of_new_block = 0;
    try {
        for (auto &row : cells) {
            auto it = row.begin() + before;
            it = row.insert(it, count, {});
            count_of_new_block++;
        }
    } catch (std::exception & excep) {
        for (int i = 0; i < count_of_new_block; i++) {
            auto it = cells[i].begin() + before;
            cells[i].erase(it, it + count);
        }
        throw excep;
    }
    size.cols += count;

    for (auto & row : cells) {
        for (int i = before + count; i < size.cols; i++) {
            if (!row[i].expired()) {
                dep_graph.InvalidOutcoming(row[i].lock());
                auto formula = dynamic_cast<DefaultFormula *>(row[i].lock().get());
                if (formula) {
                    formula->HandleInsertedRows(before, count);
                }
            }
        }
    }
}

void SpreadSheet::DeleteRows(int first, int count) {
    if (size == Size{0, 0})
        return;

    for (int i = first; i < size.rows; i++){
        for (auto & el : cells[i]){
            if (!el.expired()) {
                dep_graph.InvalidOutcoming(el.lock());
                if (i < first + count) {
                    dep_graph.Delete(el.lock());
                }
                else {
                    auto formula = dynamic_cast<DefaultFormula *>(el.lock().get());
                    if (formula) {
                        formula->HandleDeletedRows(first, count);
                    }
                }
            }
        }
    }

    cells.erase(cells.begin() + first, cells.begin() + first + count);
    size.rows = (size.rows >= count) ? size.rows - count : 0;
}

void SpreadSheet::DeleteCols(int first, int count) {
    if (size == Size{0, 0})
        return;

    for (auto & row : cells){
        for (int j = first; j < static_cast<int>(row.size()); j++){
            if (!row[j].expired()) {
                dep_graph.InvalidOutcoming(row[j].lock());
                if (j < first + count) {
                    dep_graph.Delete(row[j].lock());
                } else {
                    auto formula = dynamic_cast<DefaultFormula *>(row[j].lock().get());
                    if (formula) {
                        formula->HandleDeletedCols(first, count);
                    }
                }
            }
        }
    }

    for (auto & row : cells){
        row.erase(row.begin() + first, row.begin() + first + count);
    }
    size.cols = (size.cols >= count) ? size.cols - count : 0;
}

Size SpreadSheet::GetPrintableSize() const {
    return size;
}

DependencyGraph &SpreadSheet::GetGraph() {
    return dep_graph;
}

void SpreadSheet::PrintValues(std::ostream &output) const {
    for (int i = 0; i < size.rows; i++){
        bool is_first = true;
        for (int j = 0; j < size.cols; j++) {
            if (!is_first)
                output << '\t';
            is_first = false;
            if (auto cell = GetCell({i, j}); cell) {
                if (std::holds_alternative<double>(cell->GetValue()))
                    output << std::get<double>(cell->GetValue());
                else
                    output << std::get<std::string>(cell->GetValue());
            }
        }
        output << '\n';
    }
}

void SpreadSheet::PrintTexts(std::ostream &output) const {
    for (int i = 0; i < size.rows; i++){
        bool is_first = true;
        for (int j = 0; j < size.cols; j++) {
            if (!is_first)
                output << '\t';
            is_first = false;
            if (auto cell = GetCell({i, j}); cell) {
                output << cell->GetText();
            }
        }
        output << '\n';
    }
}
