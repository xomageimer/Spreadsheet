#include "Engine.h"

#include <algorithm>


ICell::Value DefaultCell::GetValue() const {
     if (formula_ && formula_->status != DefaultFormula::Status::Valid) {
        auto eval_val = formula_->GetValue();
        if (std::holds_alternative<double>(eval_val))
            value = std::get<double>(eval_val);
        else
            value = std::get<FormulaError>(eval_val);
    } else if (std::holds_alternative<std::string>(value)
        && std::get<std::string>(value).front() == kEscapeSign) {
        return ICell::Value(std::get<std::string>(value).substr(1));
    }
    return value;
}

std::string DefaultCell::GetText() const {
    if (formula_){
        return "=" + formula_->GetExpression();
    }
    if (std::holds_alternative<std::string>(value))
        return std::get<std::string>(value);
    else if (std::holds_alternative<double>(value)) {
        std::stringstream ss;
        auto val = std::get<double>(value);
        ss << val;
        return ss.str();
    } else
        return std::string{std::get<FormulaError>(value).ToString()};
}

std::vector<Position> DefaultCell::GetReferencedCells() const {
    if (formula_)
        return formula_->GetReferencedCells();
    return std::vector<Position>{};
}

DefaultCell::DefaultCell(const std::string &text, ISheet const * sheet) : value(Value(text)) {
    if (text.size() > 1 && text.front() == '='){
        formula_ = std::make_shared<DefaultFormula>(text.substr(1), sheet);
    } else if (AllIsDigits(text)) {
        if (!text.empty())
            value = ICell::Value(std::stod(text));
        else value = static_cast<double>(0.0);
    }
}

bool DefaultCell::AllIsDigits(const std::string &str) {
    return std::all_of(str.begin(), str.end(), [](auto c) {
        return std::isdigit(c);
    }) || (str.size() > 1 && (str.front() == '-' || str.front() == '+') && std::all_of(std::next(str.begin()), str.end(), [](auto c) {
        return std::isdigit(c);
    }));
}

DefaultFormula::DefaultFormula(std::string const & val, const ISheet * sheet) : sheet_(sheet) {
    BuildAST(val);
}

std::unique_ptr<IFormula> ParseFormula(std::string expression) {
    auto formula = std::make_unique<DefaultFormula>(expression);
    return formula;
}

IFormula::Value DefaultFormula::GetValue() const {
    auto val = Evaluate(*sheet_);
    if (status == Status::Error)
        return GetError();
    else
        return val;
}

std::vector<Position> DefaultFormula::GetReferencedCells() const {
    return as_tree->GetCellsPos();
}

std::string DefaultFormula::GetExpression() const {
    return as_tree->GetExpression(*sheet_);
}

IFormula::Value DefaultFormula::Evaluate(const ISheet &sheet) const {
    IFormula::Value val;
    try {
        val = as_tree->Evaluate(sheet);
        status = Status::Valid;
    } catch (FormulaError & fe) {
        status = Status::Error;
        error = fe;
    }
    return val;
}

IFormula::HandlingResult DefaultFormula::HandleInsertedRows(int before, int count) {
    if (GetReferencedCells().empty())
        return IFormula::HandlingResult::NothingChanged;
    return as_tree->InsertRows(before, count);
}

IFormula::HandlingResult DefaultFormula::HandleInsertedCols(int before, int count) {
    if (GetReferencedCells().empty())
        return IFormula::HandlingResult::NothingChanged;
    return as_tree->InsertCols(before, count);
}

IFormula::HandlingResult DefaultFormula::HandleDeletedRows(int first, int count) {
    if (GetReferencedCells().empty())
        return IFormula::HandlingResult::NothingChanged;
    return as_tree->DeleteRows(first, count);
}

IFormula::HandlingResult DefaultFormula::HandleDeletedCols(int first, int count) {
    if (GetReferencedCells().empty())
        return IFormula::HandlingResult::NothingChanged;
    return as_tree->DeleteCols(first, count);
}

void DefaultFormula::BuildAST(std::string const & text) const {
    if (!as_tree || (as_tree && as_tree->GetCellsPos().empty())) {
        try {
            std::stringstream ss(text);
            as_tree = std::make_shared<AST::ASTree>(AST::ParseFormula(ss, *sheet_));
        } catch (FormulaError & fe) {
            status = Status::Error;
            error = fe;
        }
    }
}

const std::shared_ptr<AST::ASTree> DefaultFormula::GetAST() const {
    return as_tree;
}

FormulaError DefaultFormula::GetError() const {
    return error;
}

void SpreadSheet::SetCell(Position pos, std::string text) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("invalid pos");
    }

    CheckSizeCorrectly(pos);

    // TODO мб написать компортаор кт будет игнорировать пробельные символы
    if (auto cell = cells.at(pos.row).at(pos.col).lock(); (cell && cell->GetText() == text))
        return;

    std::shared_ptr<DefaultCell> prev_val = nullptr;
    std::shared_ptr<DefaultCell> val = std::make_shared<DefaultCell>(text, this);
    if (auto cell = cells.at(pos.row).at(pos.col); !cell.expired()) {
        dep_graph.InvalidOutcoming(cell.lock());
        prev_val = std::make_shared<DefaultCell>(*cells.at(pos.row).at(pos.col).lock());
        dep_graph.Delete(pos, cell.lock());
    } else if (dep_graph.IsExist(pos)) {
        dep_graph.InvalidOutcoming(pos);
    }
    cells[pos.row][pos.col] = dep_graph.AddVertex(pos, val);
    val = cells[pos.row][pos.col].lock();

    try {
        if (val->GetFormula()) {
            auto & as_tree = dynamic_cast<DefaultFormula const *>(val->GetFormula().get())->GetAST();

            if (as_tree) {
                auto cells_pos = as_tree->GetCellsPos();
                for (auto &cell_pos : cells_pos) {
                    dep_graph.AddEdge(pos, cell_pos);
                }
            }
        }
    } catch (const CircularDependencyException& ex) {
        if (prev_val)
            *cells.at(pos.row).at(pos.col).lock() = *prev_val;
        throw ex;
    }
}

const ICell* SpreadSheet::GetCell(Position pos) const {
    if (!(pos < size))
        return nullptr;
    return cells.at(pos.row).at(pos.col).lock().get();
}

ICell* SpreadSheet::GetCell(Position pos) {
    if (!(pos < size))
        return nullptr;
    else if (cells.at(pos.row).at(pos.col).expired())
        return &default_value;
    return cells.at(pos.row).at(pos.col).lock().get();
}

void SpreadSheet::ClearCell(Position pos) {
    if (!pos.IsValid())
        throw InvalidPositionException("invalid pos");

    if (!(size > pos))
        return;

    if (auto cell = cells.at(pos.row).at(pos.col); !cell.expired()) {
        dep_graph.InvalidOutcoming(cell.lock());
        dep_graph.Delete(pos, cell.lock());
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
    dep_graph.InsertRows(before, count);
    size.rows += count;

    for (int i = 0; i < size.rows; i++) {
        for (auto & el : cells[i]) {
            if (!el.expired()) {
                if (i >= before + count)
                    dep_graph.InvalidOutcoming(el.lock());
                auto formula = dynamic_cast<DefaultFormula *>(el.lock()->GetFormula().get());
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
    } catch (const std::exception & excep) {
        for (int i = 0; i < count_of_new_block; i++) {
            auto it = cells[i].begin() + before;
            cells[i].erase(it, it + count);
        }
        throw excep;
    }
    dep_graph.InsertCols(before, count);
    size.cols += count;

    for (auto & row : cells) {
        for (int i = 0; i < size.cols; i++) {
            if (!row[i].expired()) {
                if (i >= before + count)
                    dep_graph.InvalidOutcoming(row[i].lock());
                auto formula = dynamic_cast<DefaultFormula *>(row[i].lock()->GetFormula().get());
                if (formula) {
                    formula->HandleInsertedCols(before, count);
                }
            }
        }
    }
}

void SpreadSheet::DeleteRows(int first, int count) {
    if (size == Size{0, 0})                 // TODO надо ли делать такую проверку?
        return;

    for (int i = 0; i < size.rows; i++){
        for (int j = 0; j < size.cols; j++) {
            auto & el = cells[i][j];
            if (!el.expired()) {
                if (i >= first + count)
                    dep_graph.InvalidOutcoming(el.lock());
                else if (i >= first && i < count) {
                    dep_graph.Delete({i, j}, el.lock());
                    continue;
                }
                auto formula = dynamic_cast<DefaultFormula *>(el.lock()->GetFormula().get());
                if (formula) {
                    formula->HandleDeletedRows(first, count);
                }
            }
        }
    }
    dep_graph.DeleteRows(first, count);

    cells.erase(cells.begin() + first, cells.begin() + first + count);
    size.rows = (size.rows >= count) ? size.rows - count : 0;
}

void SpreadSheet::DeleteCols(int first, int count) {
    if (size == Size{0, 0})
        return;

    for (int i = 0; i < size.rows; i++){
        auto & row = cells[i];
        for (int j = 0; j < size.cols; j++){
            if (!row[j].expired()) {
                if (j >= first + count)
                    dep_graph.InvalidOutcoming(row[j].lock());
                else if (j >= first && j < count) {
                    dep_graph.Delete({i, j}, row[j].lock());
                    continue;
                }
                auto formula = dynamic_cast<DefaultFormula *>(row[j].lock()->GetFormula().get());
                if (formula) {
                    formula->HandleDeletedCols(first, count);
                }
            }
        }
    }
    dep_graph.DeleteCols(first, count);

    for (auto & row : cells){
        row.erase(row.begin() + first, row.begin() + first + count);
    }
    size.cols = (size.cols >= count) ? size.cols - count : 0;
}

Size SpreadSheet::GetPrintableSize() const {
    return size;
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
                else if (std::holds_alternative<FormulaError>(cell->GetValue()))
                    output << std::get<FormulaError>(cell->GetValue()).ToString();
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

void SpreadSheet::CheckSizeCorrectly(Position pos) {
    if (size == Size{0, 0} || size < pos) {
        if (!size.rows || pos.row >= size.rows) {
            if (size.rows == Position::kMaxRows)
                throw TableTooBigException("The number of rows is greater/equal than the maximum");

            size_t i = size.rows;
            size.rows = pos.row + 1;
            cells.resize(size.rows);
            for (int j = i; j < size.rows; j++) {
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
}

SpreadSheet::SpreadSheet() : dep_graph(*this), size(Size{0, 0}), default_value("", this) {}
