#ifndef SPREADSHEET_ENGINE_H
#define SPREADSHEET_ENGINE_H

#include <unordered_map>
#include <utility>

#include "Graph.h"
#include "common.h"
#include "formula.h"

struct FormulaCell : public ICell, public IFormula {
    FormulaCell(ICell::Value text_val, ISheet * sheet = nullptr);

    IFormula::Value Evaluate(const ISheet& sheet) const override; // TODO не забыть менять статус // TODO добовлять в граф зависитмостей объекты

    std::string GetExpression() const override;

    [[nodiscard]] std::vector<Position> GetReferencedCells() const override;

    HandlingResult HandleInsertedRows(int before, int count = 1) override;
    HandlingResult HandleInsertedCols(int before, int count = 1) override;

    HandlingResult HandleDeletedRows(int first, int count = 1) override;
    HandlingResult HandleDeletedCols(int first, int count = 1) override;

    [[nodiscard]] ICell::Value GetValue() const override;

    [[nodiscard]] std::string GetText() const override;

protected:
    ISheet * sheet_;
};

struct DefaultCell : public ICell {
    using ICell::ICell;
    [[nodiscard]] Value GetValue() const override;

    [[nodiscard]] std::string GetText() const override;

    [[nodiscard]] std::vector<Position> GetReferencedCells() const override;
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
    std::vector<std::vector<std::shared_ptr<ICell>>> cells {};
    DependencyGraph dep_graph {*this};

    Size size {0, 0};
};


#endif //SPREADSHEET_ENGINE_H
