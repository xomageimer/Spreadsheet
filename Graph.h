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

struct DependencyGraph {
public:
    explicit DependencyGraph(ISheet & com_sheet) : sheet(com_sheet) {}

    std::weak_ptr<struct DefaultCell> AddVertex(std::shared_ptr<struct DefaultCell> new_cell);
    void AddEdge(std::shared_ptr<struct DefaultCell> par_cell, std::shared_ptr<struct DefaultCell> child_cell);

    void Delete(const std::shared_ptr<struct DefaultCell>& cell_ptr);

    void InvalidIncoming(std::shared_ptr<struct DefaultCell> cell_ptr);
    void InvalidOutcoming(std::shared_ptr<struct DefaultCell> cell_ptr);
private:
    std::unordered_map<std::shared_ptr<struct DefaultCell>, Edges> vertexes;
    std::vector<Edge> outcoming;
    std::vector<Edge> incoming;

    ISheet & sheet;
};

#endif //SPREADSHEET_GRAPH_H
