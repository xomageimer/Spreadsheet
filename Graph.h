#ifndef SPREADSHEET_GRAPH_H
#define SPREADSHEET_GRAPH_H

#include "common.h"

#include <vector>
#include <unordered_map>

struct Edge {
    std::weak_ptr<ICell> from;
    std::weak_ptr<ICell> to;
};
using EdgeID = size_t;
struct Edges {
    std::vector<EdgeID> incoming_ids;
    std::vector<EdgeID> outcoming_ids;
};

struct DependencyGraph {
public:
    explicit DependencyGraph(ISheet & com_sheet) : sheet(com_sheet) {}

    std::weak_ptr<ICell> AddVertex(std::shared_ptr<ICell> new_cell);
    EdgeID AddEdge(std::shared_ptr<ICell> par_cell, std::shared_ptr<ICell> child_cell);
    void Delete(const std::shared_ptr<ICell>& cell_ptr);
    void InvalidIncoming(std::shared_ptr<ICell> cell_ptr);
    void InvalidOutcoming(std::shared_ptr<ICell> cell_ptr);
private:
    std::unordered_map<std::shared_ptr<ICell>, Edges> vertexes;
    std::vector<Edge> outcoming;
    std::vector<Edge> incoming;

    ISheet & sheet;
};

#endif //SPREADSHEET_GRAPH_H
