#ifndef SPREADSHEET_GRAPH_H
#define SPREADSHEET_GRAPH_H

#include "common.h"

#include <vector>
#include <unordered_map>

struct Edge {
    std::weak_ptr<struct DefaultCell> from;
    std::weak_ptr<struct DefaultCell> to;
};
using EdgeID = size_t;
struct Edges {
    std::vector<EdgeID> incoming_ids;
    std::vector<EdgeID> outcoming_ids;
};

struct CacheVertex {
    std::shared_ptr<struct DefaultCell> cur_val;
    std::shared_ptr<struct DefaultCell> old_val = nullptr;
};

struct DependencyGraph {
public:
    explicit DependencyGraph(ISheet & com_sheet) : sheet(com_sheet) {}

    std::weak_ptr<struct DefaultCell> AddVertex(Position pos, std::shared_ptr<struct DefaultCell> new_cell);
    bool IsExist(Position pos);
    void ResetPos(Position old_pos, Position new_pos);
    void AddEdge(Position par_pos, Position child_pos);

    void Delete(const std::shared_ptr<struct DefaultCell>& cell_ptr);
    void Delete(Position pos);

    void InsertRows(int before, int count);
    void InsertCols(int before, int count);

    void DeleteRows(int first, int count);
    void DeleteCols(int first, int count);

    void InvalidIncoming(std::shared_ptr<struct DefaultCell> cell_ptr);

    void InvalidOutcoming(std::shared_ptr<struct DefaultCell> cell_ptr);
    void InvalidOutcoming(Position pos);
private:
    std::unordered_map<std::shared_ptr<struct DefaultCell>, Edges> vertexes;
    std::map<Position, CacheVertex> cache_cells_located_behind_table;

    std::vector<Edge> outcoming;
    std::vector<Edge> incoming;

    ISheet & sheet;
};

#endif //SPREADSHEET_GRAPH_H
