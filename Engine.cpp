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

bool is_str_equal(std::string_view str1, std::string_view str2) {
    for (auto it_s1 = str1.begin(), it_s2 = str2.begin(); it_s1 != str1.end(); it_s1++, it_s2++){
        while (it_s1 != str1.end() && isspace(*it_s1))
            it_s1++;
        while (it_s2 != str2.end() && isspace(*it_s2))
            it_s2++;

        if (it_s1 == str1.end() || it_s2 == str2.end())
            return false;

        if (*it_s1 != *it_s2)
            return false;
    }
    return true;
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

    if (auto cell = cells.at(pos.row).at(pos.col).lock(); (cell && is_str_equal(cell->GetText(), text)))
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
    return const_cast<SpreadSheet *>(this)->GetCell(pos);
}

// TODO ячейка - nullptr, если на нее никто не ссылается
ICell* SpreadSheet::GetCell(Position pos) {
    if (pos.row > size.rows - 1 || pos.col > size.cols - 1) {
        return nullptr;
    }
    else if (static_cast<int>(cells.at(pos.row).size()) <= pos.col){
        if (dep_graph.HasOutcomings(pos))
            return &default_value;
        else return nullptr;
    }
    auto& cell = cells.at(pos.row).at(pos.col);
    return (cell.expired() || cell.lock()->GetText().empty()) ? nullptr : cell.lock().get();
}

void SpreadSheet::ClearCell(Position pos) {
    if (!pos.IsValid())
        throw InvalidPositionException("invalid pos");

    if (!(size > pos))
        return;

    if (static_cast<int>(cells.at(pos.row).size()) > pos.col) {
        if (auto cell = cells.at(pos.row).at(pos.col); !cell.expired()) {
            dep_graph.InvalidOutcoming(cell.lock());
            dep_graph.Delete(pos, cell.lock());
            TryToCompress(pos);
        }
    }
}

void SpreadSheet::InsertRows(int before, int count) {
    if (size.rows + count >= Position::kMaxRows || dep_graph.GetMaxCachePos().row + count >= Position::kMaxRows)
        throw TableTooBigException("The number of rows is greater than the maximum");
    if (size.rows <= before)
        return;


//    std::stringstream s_;
//    PrintTexts(s_);
//    if (s_.str() != "=1\t=A2\n"
//                    "=A1\t=B1\n"
//                    "0\t=A2+B2\n" || (size.rows != 3 && size.cols != 2)) {
//        std::stringstream ss;
//        ss << "before = " << std::to_string(before) << ", count =" << std::to_string(count) << std::endl;
//        PrintTexts(ss);
//        throw std::logic_error(ss.str());
//    }

    size_t prev_size = cells.size();
    cells.resize(prev_size + count);
    for (int row = static_cast<int>(prev_size) - 1; row >= before; --row){
        std::swap(cells[row], cells[row + count]);
    }
    dep_graph.InsertRows(before, count);
    size.rows = cells.size();

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
    if (size.cols + count >= Position::kMaxCols || dep_graph.GetMaxCachePos().col + count >= Position::kMaxRows)
        throw TableTooBigException("The number of cols is greater than the maximum");
    if (size.cols <= before)
        return;

    int count_of_new_block = 0;
    try {
        for (auto &row : cells) {
            if (static_cast<int>(row.size() - 1) >= before) {
                auto it = row.begin() + before;
                it = row.insert(it, count, {});
            }
            count_of_new_block++;
        }
    } catch (const std::exception & excep) {
        for (int i = 0; i < count_of_new_block; i++) {
            if (static_cast<int>(cells[i].size() - 1) >= before) {
                auto it = cells[i].begin() + before;
                cells[i].erase(it, it + count);
            }
        }
        throw excep;
    }
    dep_graph.InsertCols(before, count);
    size.cols += count;

    for (auto & row : cells) {
        for (int i = 0; i < static_cast<int>(row.size()); i++) {
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
        for (auto col_it = cells[i].begin(); col_it != cells[i].end(); col_it++) {
            if (!col_it->expired()) {
                if (i >= first + count)
                    dep_graph.InvalidOutcoming(col_it->lock());
                else if (i >= first && i < count) {
                    dep_graph.Delete({i, static_cast<int>(col_it - cells[i].begin())}, col_it->lock());
                    dep_graph.Delete({i, static_cast<int>(col_it - cells[i].begin())});
                    continue;
                }
                auto formula = dynamic_cast<DefaultFormula *>(col_it->lock()->GetFormula().get());
                if (formula) {
                    formula->HandleDeletedRows(first, count);
                }
            }
        }
    }
    dep_graph.DeleteRows(first, count);

    cells.erase(cells.begin() + first, cells.begin() + first + count);
    size.rows = (size.rows >= count) ? size.rows - count : 0;
    if (auto pos = Position{size.rows - 1, size.cols - 1}; static_cast<int>(cells.at(pos.row).size()) > pos.col && cells.at(pos.row).at(pos.col).expired()){
        TryToCompress(pos);
    }
}

void SpreadSheet::DeleteCols(int first, int count) {
    if (size == Size{0, 0})
        return;

    for (int i = 0; i < size.rows; i++){
        for (auto col_it = cells[i].begin(); col_it != cells[i].end(); col_it++){
            if (!col_it->expired()) {
                if (static_cast<int>(col_it - cells[i].begin()) >= first + count)
                    dep_graph.InvalidOutcoming(col_it->lock());
                else if (static_cast<int>(col_it - cells[i].begin()) >= first && static_cast<int>(col_it - cells[i].begin()) < count) {
                    dep_graph.Delete({i, static_cast<int>(col_it - cells[i].begin())}, col_it->lock());
                    dep_graph.Delete({i, static_cast<int>(col_it - cells[i].begin())});
                    continue;
                }
                auto formula = dynamic_cast<DefaultFormula *>(col_it->lock()->GetFormula().get());
                if (formula) {
                    formula->HandleDeletedCols(first, count);
                }
            }
        }
    }
    dep_graph.DeleteCols(first, count);

    for (auto & row : cells){
        if (static_cast<int>(row.size()) >= first)
            row.erase(row.begin() + first, row.begin() + first + count);
    }
    size.cols = (size.cols >= count) ? size.cols - count : 0;
    if (auto pos = Position{size.rows - 1, size.cols - 1}; static_cast<int>(cells.at(pos.row).size()) > pos.col && cells.at(pos.row).at(pos.col).expired()){
        TryToCompress(pos);
    }
}

Size SpreadSheet::GetPrintableSize() const {
    return size;
}

void SpreadSheet::PrintValues(std::ostream &output) const {
    for (int i = 0; i < size.rows; i++){
        for (int j = 0; j < size.cols; j++) {
            if (static_cast<int>(cells[i].size()) > j) {
                if (auto cell = GetCell({i, j}); cell) {
                    if (std::holds_alternative<double>(cell->GetValue()))
                        output << std::get<double>(cell->GetValue());
                    else if (std::holds_alternative<FormulaError>(cell->GetValue()))
                        output << std::get<FormulaError>(cell->GetValue()).ToString();
                    else
                        output << std::get<std::string>(cell->GetValue());
                }
            }
            if (j != size.cols - 1)
                output << '\t';
        }
        output << '\n';
    }
}

void SpreadSheet::PrintTexts(std::ostream &output) const {
    for (int i = 0; i < size.rows; i++){
        for (int j = 0; j < size.cols; j++) {
            if (static_cast<int>(cells[i].size()) > j) {
                if (auto cell = GetCell({i, j}); cell) {
                    output << cell->GetText();
                }
            }
            if (j != size.cols - 1)
                output << '\t';
        }
        output << '\n';
    }
}

void SpreadSheet::CheckSizeCorrectly(Position pos) {
    if (size.rows <= pos.row) {
        if (size.rows == Position::kMaxRows)
            throw TableTooBigException("The number of rows is greater/equal than the maximum");

        size.rows = pos.row + 1;
        cells.resize(size.rows);
    }
    if (cells[pos.row].size() <= static_cast<size_t>(pos.col)){
        if (size.cols == Position::kMaxCols)
            throw TableTooBigException("The number of cols is greater/equal than the maximum");

        cells[pos.row].resize(pos.col + 1);
    }
    if (size.cols <= pos.col)
        size.cols = pos.col + 1;
}

SpreadSheet::SpreadSheet() : dep_graph(*this), size(Size{0, 0}), default_value("", this) {}

void SpreadSheet::TryToCompress(Position from_pos) {
    if (from_pos.col == size.cols - 1) {
        std::optional<int> max_pos;

        for (int i = static_cast<int>(size.rows) - 1; i >= 0; i--){
            for (auto col_it = cells[i].rbegin(); col_it != cells[i].rend(); col_it++){
                if (!col_it->expired() && !dep_graph.IsExist(Position{i, static_cast<int>(std::prev(cells[i].rend()) - col_it)}) && (!max_pos || std::prev(cells[i].rend()) - col_it > max_pos.value())) {
                    max_pos.emplace(std::prev(cells[i].rend()) - col_it);
                    break;
                }
            }
        }

        for (auto & raw : cells) {
            int to_pos = (!max_pos) ? 0 : (*max_pos + 1);
            if (static_cast<int>(raw.size()) >= to_pos)
                raw.erase(raw.begin() + to_pos, raw.end());
        }
        size.cols = (!max_pos) ? 0 : (*max_pos + 1);
    }
    if (from_pos.row == size.rows - 1) {
        std::optional<int> max_pos;

        for (int j = size.rows - 1; j >= 0; j--){
            int i = 0;
            for (auto & col : cells[j]){
                if (!col.expired() && !dep_graph.IsExist(Position{j, i}) && (!max_pos || j > max_pos.value())) {
                    max_pos.emplace(j);
                }
                i++;
            }
        }

        cells.erase(cells.begin() + ((!max_pos) ? 0 : (*max_pos + 1)), cells.end());
        size.rows = (!max_pos) ? 0 : (*max_pos + 1);
    }
}
