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
double Cell::Evaluate() const {
    auto cell = sheet_.GetCell(pos_);
    if (!cell)
        return 0;
    return std::get<double>(sheet_.GetCell(pos_)->GetValue());
}

UnaryOp::UnaryOp(char op) {
    switch (op) {
        case '+' :
            op_ = type::UN_ADD;
            break;
        case '-' :
            op_ = type::UN_SUB;
            break;
        default :
            throw std::logic_error("invalid value of the unary operator");
    }
}

void UnaryOp::SetValue(std::shared_ptr<const Node> node) {
    value_ = std::move(node);
}

double UnaryOp::Evaluate() const {
    return (op_ == type::UN_SUB) ? -1 * value_->Evaluate() : value_->Evaluate();
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
};

BinaryOp::BinaryOp(char op) {
    switch (op) {
        case '+' :
            op_ = type::ADD;
            break;
        case '-' :
            op_ = type::SUB;
            break;
        case '*' :
            op_ = type::MUL;
            break;
        case '/' :
            op_ = type::DIV;
            break;
        default:
            throw std::logic_error("invalid value of the binary operator");
    }
}

void BinaryOp::SetLeft(std::shared_ptr<const Node> lhs_node) {
    left_ = std::move(lhs_node);
}

void BinaryOp::SetRight(std::shared_ptr<const Node> rhs_node) {
    right_ = std::move(rhs_node);
}

double BinaryOp::Evaluate() const {
    switch (op_) {
        case type::ADD:
            return left_->Evaluate() + right_->Evaluate();
        case type::SUB:
            return left_->Evaluate() - right_->Evaluate();
        case type::MUL:
            return left_->Evaluate() * right_->Evaluate();
        case type::DIV: {
            auto right_val = right_->Evaluate();
            if (!right_val)
                throw FormulaError(FormulaError::Category::Div0);
            return left_->Evaluate() / right_->Evaluate();
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

void ASTree::MutateRows(int from, int count) {
    cells.clear();
    for (auto & cell : cell_ptrs) {
        auto pos = cell->GetPos();
        if (pos.row > from) {
            cell->SetPos({pos.row + count, pos.col});
        }
        cells.push_back(cell->GetPos());
    }
    std::sort(cells.begin(), cells.end());
}

void ASTree::MutateCols(int from, int count) {
    cells.clear();
    for (auto & cell : cell_ptrs) {
        auto pos = cell->GetPos();
        if (pos.col > from) {
            cell->SetPos({pos.row, pos.col + count});
        }
        cells.push_back(cell->GetPos());
    }
    std::sort(cells.begin(), cells.end());
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

ASTree ParseFormula(std::istream & in, const ISheet & sheet) {
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
