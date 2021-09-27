#ifndef SPREADSHEET_GRAPH_H
#define SPREADSHEET_GRAPH_H

#include "common.h"

#include <algorithm>
#include <memory>
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>

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
};

struct DependencyGraph {
public:
    explicit DependencyGraph(ISheet & com_sheet) : sheet(com_sheet) {}

    std::weak_ptr<struct DefaultCell> AddVertex(Position pos, std::shared_ptr<struct DefaultCell> new_cell);
    bool IsExist(Position pos);
    void AddEdge(Position par_pos, Position child_pos);

    void Delete(Position pos, const std::shared_ptr<struct DefaultCell>& cell_ptr);
    void Delete(Position pos);

    void InsertRows(int before, int count);
    void InsertCols(int before, int count);

    void DeleteRows(int first, int count);
    void DeleteCols(int first, int count);

    void InvalidIncoming(std::shared_ptr<struct DefaultCell> cell_ptr);

    void InvalidOutcoming(std::shared_ptr<struct DefaultCell> cell_ptr);
    void InvalidOutcoming(Position pos);

    bool HasOutcomings(Position pos);

    Position GetMaxCachePos() const;
private:
    std::unordered_map<std::shared_ptr<struct DefaultCell>, Edges> vertexes;
    std::map<Position, CacheVertex> cache_cells_located_behind_table;

    std::unordered_map<int, Edge> outcoming;
    std::unordered_map<int, Edge> incoming;
    int c = 0;

    ISheet & sheet;

    void CheckAcyclicity(std::shared_ptr<struct DefaultCell> cell_ptr);
    void CheckAcylicityImpl(std::shared_ptr<struct DefaultCell> cell_ptr, std::unordered_set<std::shared_ptr<struct DefaultCell>> & cols);
};

#endif //SPREADSHEET_GRAPH_H
