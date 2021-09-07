#ifndef SPREADSHEET_AST_H
#define SPREADSHEET_AST_H

#include <utility>
#include <stack>
#include <sstream>

#include "common.h"
#include "formula.h"

#include "FormulaLexer.h"
#include "FormulaBaseListener.h"
#include "FormulaParser.h"

class BailErrorListener : public antlr4::BaseErrorListener {
public:
    void syntaxError(antlr4::Recognizer* /* recognizer */,
                     antlr4::Token* /* offendingSymbol */, size_t /* line */,
                     size_t /* charPositionInLine */, const std::string& msg,
                     std::exception_ptr /* e */
    ) override {
        throw std::runtime_error("Error when lexing: " + msg);
    }
};

namespace AST {
    enum type {
        ADD,
        SUB,
        MUL,
        DIV,
        UN_ADD,
        UN_SUB,
        ATOM
    };
    struct Node {
        [[nodiscard]] virtual IFormula::Value Evaluate(const ISheet &) const = 0;
        [[nodiscard]] virtual std::string GetText(const ISheet &) const = 0;
        [[nodiscard]] virtual type GetOpType() const {return op_;}
    protected:
        type op_;
    };

    enum class NeedOfBrackets {
        NO_NEED = 0b00,
        RIGHT = 0b01,
        LEFT = 0b10,
        BOTH = RIGHT | LEFT
    };
    using nob = NeedOfBrackets;

    static const NeedOfBrackets table_of_necessity [type::ATOM + 1][type::ATOM + 1]{
            {nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED}, // . + .
            {nob::RIGHT, nob::RIGHT, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED},     // . - .
            {nob::BOTH, nob::BOTH, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED},       // . * .
            {nob::BOTH, nob::BOTH, nob::RIGHT, nob::RIGHT, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED},           // ./.
            {nob::BOTH, nob::BOTH, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED},       // +.
            {nob::BOTH, nob::BOTH, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED},       // -.
            {nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED, nob::NO_NEED}  // ATOM
    };

    struct Value : public Node {
    public:
        explicit Value(std::string const & number) : value_(std::stod(number)) { op_ = type::ATOM; }
        [[nodiscard]] IFormula::Value Evaluate(const ISheet &) const override { return value_; }
        [[nodiscard]] std::string GetText(const ISheet &) const override;
    private:
        const double value_;
    };

    struct Cell : public Node {
    public:
        explicit Cell(std::string const & pos_str) {
            op_ = type::ATOM;
            pos_ = Position::FromString(pos_str);
            if (!pos_.IsValid())
                throw FormulaException("invalid pos");
        }
        [[nodiscard]] IFormula::Value Evaluate(const ISheet &) const override;
        [[nodiscard]] std::string GetText(const ISheet &) const override;
        [[nodiscard]] Position GetPos() const { return pos_; }
        void SetPos(Position new_pos) { pos_ = new_pos; }
    private:
        Position pos_;
    };

    struct UnaryOp : public Node {
    public:
        explicit UnaryOp(type op);
        void SetValue(std::shared_ptr<const Node> node);

        [[nodiscard]] IFormula::Value Evaluate(const ISheet &) const override;
        [[nodiscard]] std::string GetText(const ISheet &) const override;
    private:
        std::shared_ptr<const Node> value_;
        [[nodiscard]] bool is_brace_needed() const;
    };

    struct BinaryOp : public Node {
    public:
        static const inline std::map<type, std::string> sign{
                {type::ADD, "+"},
                {type::SUB, "-"},
                {type::MUL, "*"},
                {type::DIV, "/"}
        };

        explicit BinaryOp(type op);
        void SetLeft(std::shared_ptr<const Node> lhs_node);
        void SetRight(std::shared_ptr<const Node> rhs_node);

        [[nodiscard]] IFormula::Value Evaluate(const ISheet &) const override;
        [[nodiscard]] std::string GetText(const ISheet &) const override;
    private:
        std::shared_ptr<const Node> left_, right_;
        [[nodiscard]] bool is_brace_needed_left() const;
        [[nodiscard]] bool is_brace_needed_right() const;
    };

    struct ASTree {
    public:
        explicit ASTree(std::shared_ptr<const Node> root_node, std::vector<std::shared_ptr<Cell>> ptrs) : root_(std::move(root_node)), cell_ptrs(std::move(ptrs)) {
            for (auto & cell : cell_ptrs) {
                cells.push_back(cell->GetPos());
            }
            std::sort(cells.begin(), cells.end());
            cells.erase(std::unique(cells.begin(), cells.end()), cells.end());
        }
        [[nodiscard]] std::string GetExpression(const ISheet & sheet) const { return root_->GetText(sheet); }
        [[nodiscard]] IFormula::Value Evaluate(const ISheet &) const;
        [[nodiscard]] std::vector<Position> GetCellsPos() const {
            return cells;
        }

        IFormula::HandlingResult InsertRows(int before, int count);
        IFormula::HandlingResult InsertCols(int before, int count);

        IFormula::HandlingResult DeleteRows(int first, int count);
        IFormula::HandlingResult DeleteCols(int first, int count);
    private:
        std::shared_ptr<const Node> root_;
        std::vector<std::shared_ptr<Cell>> cell_ptrs;
        std::vector<Position> cells;
    };

    struct ASTListener final : public FormulaBaseListener {
    public:
        explicit ASTListener() = default;

        void exitUnaryOp(FormulaParser::UnaryOpContext * op/*ctx*/) override;
        void exitCell(FormulaParser::CellContext * cell/*ctx*/) override;
        void exitLiteral(FormulaParser::LiteralContext * num /*ctx*/) override;
        void exitBinaryOp(FormulaParser::BinaryOpContext * op /*ctx*/) override;

        [[nodiscard]] ASTree Build() const;
    private:
        std::stack<std::shared_ptr<Node>> prior_ops;
        std::vector<std::shared_ptr<Cell>> cells;

        void Pop(size_t count);
    };

    ASTree ParseFormula(std::istream & in, const ISheet & sheet);
}


#endif //SPREADSHEET_AST_H
