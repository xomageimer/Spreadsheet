#include "AST.h"
#include <cmath>

using namespace AST;

std::string Value::GetText(const ISheet &) const {
    std::ostringstream ss;
    ss << value_;
    return ss.str();
}

std::string Cell::GetText(const ISheet & sheet) const {
    auto text = pos_.ToString();
    if (pos_.row < 0 || pos_.col < 0)
        text = FormulaError(FormulaError::Category::Ref).ToString();
    else
        text = pos_.ToString();
    return text;
}

IFormula::Value Cell::Evaluate(const ISheet & sheet) const {
    if (pos_.row < 0 || pos_.col < 0){
        return FormulaError::Category::Ref;
    }
    auto cell = sheet.GetCell(pos_);
    if (!cell)
        return static_cast<double>(0.0);

    auto cell_val = cell->GetValue();
    if (std::holds_alternative<std::string>(cell_val)){
        return FormulaError::Category::Value;
    }
    else if (std::holds_alternative<FormulaError>(cell_val)) {
        return std::get<FormulaError>(cell_val);
    } else
        return std::get<double>(cell_val);
}

UnaryOp::UnaryOp(type op) {
    op_ = op;
}

void UnaryOp::SetValue(std::shared_ptr<const Node> node) {
    value_ = std::move(node);
}

IFormula::Value UnaryOp::Evaluate(const ISheet & sheet) const {
    auto eval_val = value_->Evaluate(sheet);
    if (std::holds_alternative<FormulaError>(eval_val))
        return std::get<FormulaError>(eval_val);
    return (op_ == type::UN_SUB) ? -1 * std::get<double>(eval_val) : std::get<double>(eval_val);
}

std::string UnaryOp::GetText(const ISheet & sheet) const {
    if (op_ == type::UN_SUB) {
        return '-' + ((is_brace_needed()) ? "(" + value_->GetText(sheet) + ")" : value_->GetText(sheet));
    } else if (op_ == type::UN_ADD) {
        return '+' + ((is_brace_needed()) ? "(" + value_->GetText(sheet) + ")" : value_->GetText(sheet));
    }

    return value_->GetText(sheet);
}

bool UnaryOp::is_brace_needed() const {
    NeedOfBrackets brace_type = table_of_necessity[GetOpType()][value_->GetOpType()];
    switch (brace_type) {
        case nob::NO_NEED:
            return false;
        default:
            return true;
    }
}

BinaryOp::BinaryOp(type op) {
    op_ = op;
}

void BinaryOp::SetLeft(std::shared_ptr<const Node> lhs_node) {
    left_ = std::move(lhs_node);
}

void BinaryOp::SetRight(std::shared_ptr<const Node> rhs_node) {
    right_ = std::move(rhs_node);
}

IFormula::Value BinaryOp::Evaluate(const ISheet & sheet) const {
    auto lhs_val = left_->Evaluate(sheet);
    auto rhs_val = right_->Evaluate(sheet);
    if (std::holds_alternative<FormulaError>(lhs_val)) {
        return FormulaError(std::get<FormulaError>(lhs_val));
    } else if (std::holds_alternative<FormulaError>(rhs_val)) {
        return FormulaError(std::get<FormulaError>(rhs_val));
    }

    double value;
    switch (op_) {
        case type::ADD:
            value = std::get<double>(lhs_val) + std::get<double>(rhs_val);
            break;
        case type::SUB:
            value = std::get<double>(lhs_val) - std::get<double>(rhs_val);
            break;
        case type::MUL:
            value = std::get<double>(lhs_val) * std::get<double>(rhs_val);
            break;
        case type::DIV: {
            value = std::get<double>(lhs_val) / std::get<double>(rhs_val);
            break;
        }
        default:
            throw std::logic_error("invalid value of the binary operator");
    }
    if (!std::isfinite(value))
        return FormulaError(FormulaError::Category::Div0);
    return value;
}

std::string BinaryOp::GetText(const ISheet & sheet) const {
    std::string expr_text =
            (is_brace_needed_left() ? '(' + left_->GetText(sheet) + ')' : left_->GetText(sheet))
            + sign.at(op_)
            + (is_brace_needed_right() ? '(' + right_->GetText(sheet) + ')' : right_->GetText(sheet));
    return expr_text;
}

bool BinaryOp::is_brace_needed_left() const {
    NeedOfBrackets brace_type_left = table_of_necessity[GetOpType()][left_->GetOpType()];
    switch (brace_type_left) {
        case nob::BOTH:
        case nob::LEFT:
            return true;
        default:
            return false;
    }
}

bool BinaryOp::is_brace_needed_right() const {
    NeedOfBrackets brace_type_right = table_of_necessity[GetOpType()][right_->GetOpType()];
    switch (brace_type_right) {
        case nob::BOTH:
        case nob::RIGHT:
            return true;
        default:
            return false;
    }
}

IFormula::Value ASTree::Evaluate(const ISheet & sheet) const {
    return root_->Evaluate(sheet);
}

IFormula::HandlingResult ASTree::InsertRows(int before, int count) {
    IFormula::HandlingResult handle_type = IFormula::HandlingResult::NothingChanged;

    cells.clear();
    for (auto & cell : cell_ptrs) {
        auto pos = cell->GetPos();
        if (pos.row >= before) {
            cell->SetPos({pos.row + count, pos.col});
            if (handle_type == IFormula::HandlingResult::NothingChanged){
                handle_type = IFormula::HandlingResult::ReferencesRenamedOnly;
            }
        }
        cells.push_back(cell->GetPos());
    }
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
    return handle_type;
}

IFormula::HandlingResult ASTree::InsertCols(int before, int count) {
    IFormula::HandlingResult handle_type = IFormula::HandlingResult::NothingChanged;

    cells.clear();
    for (auto & cell : cell_ptrs) {
        auto pos = cell->GetPos();
        if (pos.col >= before) {
            cell->SetPos({pos.row, pos.col + count});
            if (handle_type == IFormula::HandlingResult::NothingChanged){
                handle_type = IFormula::HandlingResult::ReferencesRenamedOnly;
            }
        }
        cells.push_back(cell->GetPos());
    }
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
    return handle_type;
}

IFormula::HandlingResult ASTree::DeleteRows(int first, int count) {
    IFormula::HandlingResult handle_type = IFormula::HandlingResult::NothingChanged;

    cells.clear();
    for (auto & cell : cell_ptrs){
        auto pos = cell->GetPos();
        if (pos.row >= first) {
            cell->SetPos({pos.row - count, pos.col});
            if (pos.row < first + count) {
                handle_type = IFormula::HandlingResult::ReferencesChanged;
                cell->SetPos({pos.row - pos.row - 1, pos.col});
            } else {
                if (handle_type == IFormula::HandlingResult::NothingChanged) {
                    handle_type = IFormula::HandlingResult::ReferencesRenamedOnly;
                }
            }
        }
        if (cell->GetPos().row >= 0 && cell->GetPos().col >= 0)
            cells.push_back(cell->GetPos());
    }
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());

    return handle_type;
}

IFormula::HandlingResult ASTree::DeleteCols(int first, int count) {
    IFormula::HandlingResult handle_type = IFormula::HandlingResult::NothingChanged;

    cells.clear();
    for (auto & cell : cell_ptrs){
        auto pos = cell->GetPos();
        if (pos.col >= first) {
            cell->SetPos({pos.row, pos.col - count});
            if (pos.col < first + count) {
                handle_type = IFormula::HandlingResult::ReferencesChanged;
                cell->SetPos({pos.row, pos.col - pos.col - 1});
            } else {
                if (handle_type == IFormula::HandlingResult::NothingChanged) {
                    handle_type = IFormula::HandlingResult::ReferencesRenamedOnly;
                }
            }
        }
        if (cell->GetPos().col >= 0 && cell->GetPos().row >= 0)
            cells.push_back(cell->GetPos());
    }
    std::sort(cells.begin(), cells.end());
    cells.erase(std::unique(cells.begin(), cells.end()), cells.end());

    return handle_type;
}

void ASTListener::exitUnaryOp(FormulaParser::UnaryOpContext *op) {
    std::shared_ptr<Node> un_op;
    if (op->ADD()) {
        un_op = std::make_shared<UnaryOp>(type::UN_ADD);
    } else if (op->SUB()) {
        un_op = std::make_shared<UnaryOp>(type::UN_SUB);
    }
    prior_ops.emplace(un_op);
    Pop(1);
}

void ASTListener::exitCell(FormulaParser::CellContext *cell) {
    auto cell_ptr = std::make_shared<Cell>(cell->getText());
    cells.push_back(cell_ptr);
    prior_ops.emplace(cell_ptr);
}

void ASTListener::exitLiteral(FormulaParser::LiteralContext *num) {
    prior_ops.emplace(std::make_shared<Value>(num->getText()));
}

void ASTListener::exitBinaryOp(FormulaParser::BinaryOpContext *op) {
    std::shared_ptr<Node> bin_op;
    if (op->ADD()) {
        bin_op = std::make_shared<BinaryOp>(type::ADD);
    } else if (op->MUL()) {
        bin_op = std::make_shared<BinaryOp>(type::MUL);
    } else if (op->SUB()) {
        bin_op = std::make_shared<BinaryOp>(type::SUB);
    } else if (op->DIV()) {
        bin_op = std::make_shared<BinaryOp>(type::DIV);
    }
    prior_ops.emplace(bin_op);
    Pop(2);
}

ASTree ASTListener::Build() const {
    if (prior_ops.size() == 1)
        return ASTree{prior_ops.top(), cells};
    throw FormulaException("incorrect formula behaviour");
}

void ASTListener::Pop(size_t count) {
    auto op = prior_ops.top();
    prior_ops.pop();
    switch (count) {
        case 1 : {
            auto value = prior_ops.top();
            prior_ops.pop();
            dynamic_cast<UnaryOp *>(op.get())->SetValue(value);
            break;
        }
        case 2 : {
            auto rhs_value = prior_ops.top();
            prior_ops.pop();
            auto lhs_value = prior_ops.top();
            prior_ops.pop();

            auto bin_op = dynamic_cast<BinaryOp *>(op.get());
            bin_op->SetLeft(lhs_value);
            bin_op->SetRight(rhs_value);
            break;
        }
        default:
            break;
    }
    prior_ops.push(op);
}

ASTree AST::ParseFormula(std::istream &in, const ISheet &sheet) {
    antlr4::ANTLRInputStream input(in);

    FormulaLexer lexer(&input);
    try {
        BailErrorListener error_listener;
        lexer.removeErrorListeners();
        lexer.addErrorListener(&error_listener);

        ASTListener listener;
        antlr4::CommonTokenStream tokens(&lexer);

        FormulaParser parser(&tokens);
        auto error_handler = std::make_shared<antlr4::BailErrorStrategy>();
        parser.setErrorHandler(error_handler);
        parser.removeErrorListeners();

        antlr4::tree::ParseTree *tree = parser.main();  // ?????????? ?????????????????????????? ?????????????????? ??????????????
        antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);
        return listener.Build();
    } catch (...) {
        throw FormulaException("incorrect syntax");
    }

}