#ifndef SPREADSHEET_ENGINE_H
#define SPREADSHEET_ENGINE_H

#include <unordered_map>
#include <utility>

#include "Graph.h"
#include "AST.h"
#include "common.h"
#include "formula.h"

struct DefaultFormula : public IFormula {
    enum class Status {
        Invalid,
        Error,
        Valid
    } mutable status = Status::Invalid;

    explicit DefaultFormula(std::string const & val, const ISheet * sheet = nullptr);

    Value GetValue() const;

    IFormula::Value Evaluate(const ISheet& sheet) const override;

    std::string GetExpression() const override;

    [[nodiscard]] std::vector<Position> GetReferencedCells() const override;

    HandlingResult HandleInsertedRows(int before, int count = 1) override;
    HandlingResult HandleInsertedCols(int before, int count = 1) override;

    HandlingResult HandleDeletedRows(int first, int count = 1) override;
    HandlingResult HandleDeletedCols(int first, int count = 1) override;

    const std::optional<AST::ASTree> & GetAST() const;

    friend std::unique_ptr<IFormula> ParseFormula(std::string expression);
protected:
    mutable Value evaluated_value;
    mutable std::optional<AST::ASTree> as_tree;
    const ISheet * sheet_;

    void BuildAST(std::string const & text) const;
};

struct DefaultCell : public ICell {
    explicit DefaultCell(std::string const & text, ISheet const * sheet = nullptr);
    [[nodiscard]] Value GetValue() const override;

    [[nodiscard]] std::string GetText() const override;

    [[nodiscard]] std::vector<Position> GetReferencedCells() const override;

    [[nodiscard]] std::shared_ptr<DefaultFormula> GetFormula() const {
        return formula_;
    }
private:
    Value value;
    std::shared_ptr<DefaultFormula> formula_ = nullptr;

    bool AllIsDigits(std::string const & str);
};

struct SpreadSheet : public ISheet {
public:
    void SetCell(Position pos, std::string text) override;

    const ICell* GetCell(Position pos) const override;
    ICell* GetCell(Position pos) override;

    void ClearCell(Position pos) override;

    void InsertRows(int before, int count = 1) override;
    void InsertCols(int before, int count = 1) override;

    void DeleteRows(int first, int count = 1) override;
    void DeleteCols(int first, int count = 1) override;

    [[nodiscard]] Size GetPrintableSize() const override;

    void PrintValues(std::ostream& output) const override;
    void PrintTexts(std::ostream& output) const override;
private:
    void CheckSizeCorrectly(Position pos);

    friend DependencyGraph;

    std::vector<std::vector<std::weak_ptr<DefaultCell>>> cells {};
    mutable DependencyGraph dep_graph {*this};

    Size size {0, 0};

    DefaultCell default_value {"", this};
};

bool operator<(const Size & lhs, const Position & rhs);
bool operator<(const Position & lhs, const Size & rhs);
bool operator>(const Size & lhs, const Position & rhs);
bool operator>(const Position & lhs, const Size & rhs);
bool operator==(const Size & lhs, const Position & rhs);
bool operator==(const Position & lhs, const Size & rhs);


#endif //SPREADSHEET_ENGINE_H
