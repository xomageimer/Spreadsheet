#include "AST.h"

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
        case type::DIV:
            return left_->Evaluate() / right_->Evaluate();
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

double ASTree::Evaluate() const {
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