#ifndef SPREADSHEET_ENGINE_H
#define SPREADSHEET_ENGINE_H

#include <unordered_map>
#include <utility>

#include "Graph.h"
#include "AST.h"
#include "common.h"
#include "formula.h"

struct DefaultFormula : public IFormula {
    explicit DefaultFormula(std::string const & val, const ISheet * sheet = nullptr);

    Value GetValue() const override;

    IFormula::Value Evaluate(const ISheet& sheet) const override; // TODO не забыть менять статус // TODO добовлять в граф зависитмостей объекты

    std::string GetExpression() const override;

    [[nodiscard]] std::vector<Position> GetReferencedCells() const override;

    HandlingResult HandleInsertedRows(int before, int count = 1) override;
    HandlingResult HandleInsertedCols(int before, int count = 1) override;

    HandlingResult HandleDeletedRows(int first, int count = 1) override;
    HandlingResult HandleDeletedCols(int first, int count = 1) override;

    friend std::unique_ptr<IFormula> ParseFormula(const std::string& expression);
protected:
    mutable std::optional<AST::ASTree> as_tree;
    const ISheet * sheet_;

    void BuildAST(std::string const & text) const;
};

struct DefaultCell : public ICell {
    explicit DefaultCell(std::string const & text, ISheet const & sheet);
    [[nodiscard]] Value GetValue() const override;

    [[nodiscard]] std::string GetText() const override;

    [[nodiscard]] std::vector<Position> GetReferencedCells() const override;

    [[nodiscard]] std::shared_ptr<IFormula> GetFormula() const {
        return formula_;
    }
private:
    std::shared_ptr<IFormula> formula_ = nullptr;
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

    DependencyGraph & GetGraph();
private:
    std::vector<std::vector<std::weak_ptr<DefaultCell>>> cells {};
    DependencyGraph dep_graph {*this};

    Size size {0, 0};
};


#endif //SPREADSHEET_ENGINE_H
