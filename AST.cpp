#include "AST.h"

using namespace AST;

std::string Cell::GetText() const {
    auto text = pos_.ToString();
    if (std::holds_alternative<FormulaError>(sheet_.GetCell(pos_)->GetValue())) {
        text = std::get<FormulaError>(sheet_.GetCell(pos_)->GetValue()).ToString();
    }
    return text;
}

// TODO в случае ошибки надо в Evaluate возвращать FormulaError
IFormula::Value Cell::Evaluate() const {
    auto cell = sheet_.GetCell(pos_);
    if (!cell)
        return 0;
    return std::get<double>(sheet_.GetCell(pos_)->GetValue());
}

UnaryOp::UnaryOp(type op) {
    op_ = op;
}

void UnaryOp::SetValue(std::shared_ptr<const Node> node) {
    value_ = std::move(node);
}

IFormula::Value UnaryOp::Evaluate() const {
    return (op_ == type::UN_SUB) ? -1 * std::get<double>(value_->Evaluate()) : std::get<double>(value_->Evaluate());
}

std::string UnaryOp::GetText() const {
    return (op_ == type::UN_SUB) ? '-' + ((is_brace_needed()) ? "(" + value_->GetText() + ")" : value_->GetText()) : value_->GetText();
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

IFormula::Value BinaryOp::Evaluate() const {
    auto lhs_val = left_->Evaluate();
    auto rhs_val = right_->Evaluate();
    if (std::holds_alternative<FormulaError>(lhs_val)) {
        return FormulaError(std::get<FormulaError>(lhs_val));
    } else if (std::holds_alternative<FormulaError>(rhs_val)) {
        return FormulaError(std::get<FormulaError>(rhs_val));
    }

    switch (op_) {
        case type::ADD:
            return std::get<double>(lhs_val) + std::get<double>(rhs_val);
        case type::SUB:
            return std::get<double>(lhs_val) - std::get<double>(rhs_val);
        case type::MUL:
            return std::get<double>(lhs_val) * std::get<double>(rhs_val);
        case type::DIV: {
            if (!std::get<double>(rhs_val))
                return FormulaError(FormulaError::Category::Div0);
            return std::get<double>(lhs_val) / std::get<double>(rhs_val);
        }
        default:
            throw std::logic_error("invalid value of the binary operator");
    }
}

std::string BinaryOp::GetText() const {
    std::string expr_text = left_->GetText() + sign.at(op_) + right_->GetText();
    return is_brace_needed() ? "(" + expr_text + ")" : expr_text;
}

bool BinaryOp::is_brace_needed() const {
    NeedOfBrackets brace_type_left = table_of_necessity[GetOpType()][left_->GetOpType()];
    NeedOfBrackets brace_type_right = table_of_necessity[GetOpType()][right_->GetOpType()];
    switch (brace_type_left) {
        case nob::BOTH:
        case nob::LEFT:
            return true;
        default:
            break;
    }
    switch (brace_type_right) {
        case nob::BOTH:
        case nob::RIGHT:
            return true;
        default:
            return false;
    }
}

IFormula::Value ASTree::Evaluate() const {
    return root_->Evaluate();
}

IFormula::HandlingResult ASTree::MutateRows(int from, int count) {
    IFormula::HandlingResult handle_type = IFormula::HandlingResult::NothingChanged;

    cells.clear();
    for (auto & cell : cell_ptrs) {
        auto pos = cell->GetPos();
        if (pos.row >= from) {
            cell->SetPos({pos.row + count, pos.col});
            if (handle_type == IFormula::HandlingResult::NothingChanged){
                handle_type = IFormula::HandlingResult::ReferencesRenamedOnly;
            }
        } else {
            handle_type = IFormula::HandlingResult::ReferencesChanged;
        }
        cells.push_back(cell->GetPos());
    }
    std::sort(cells.begin(), cells.end());
    return handle_type;
}

// TODO при удалении надо как-то чтоле убирать значения
IFormula::HandlingResult ASTree::MutateCols(int from, int count) {
    IFormula::HandlingResult handle_type = IFormula::HandlingResult::NothingChanged;

    cells.clear();
    for (auto & cell : cell_ptrs) {
        auto pos = cell->GetPos();
        if (pos.col >= from) {
            cell->SetPos({pos.row, pos.col + count});
            if (handle_type == IFormula::HandlingResult::NothingChanged){
                handle_type = IFormula::HandlingResult::ReferencesRenamedOnly;
            }
        } else {
            handle_type = IFormula::HandlingResult::ReferencesChanged;
        }
        cells.push_back(cell->GetPos());
    }
    std::sort(cells.begin(), cells.end());
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
    auto cell_ptr = std::make_shared<Cell>(cell->getText(), *sheet_);
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
    } catch (...)
    {
        throw FormulaException("Invalid formula");
    }

    antlr4::CommonTokenStream tokens(&lexer);

    FormulaParser parser(&tokens);
    auto error_handler = std::make_shared<antlr4::BailErrorStrategy>();
    parser.setErrorHandler(error_handler);
    parser.removeErrorListeners();

    antlr4::tree::ParseTree *tree = parser.main();  // метод соответствует корневому правилу
    ASTListener listener(sheet);
    antlr4::tree::ParseTreeWalker::DEFAULT.walk(&listener, tree);

    return listener.Build();
}
