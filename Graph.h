#ifndef SPREADSHEET_GRAPH_H
#define SPREADSHEET_GRAPH_H

#include "common.h"

#include <vector>
#include <unordered_map>

struct Edge {
    Position from;
    Position to;
};
using EdgeID = size_t;
struct Edges {
    std::vector<EdgeID> incoming_ids;
    std::vector<EdgeID> outcoming_ids;
};

struct DependencyGraph {
public:
    explicit DependencyGraph(ISheet & com_sheet) : sheet(com_sheet) {}
    EdgeID AddEdge(Position parent, Position child);

    void Delete(Position pos);
    void Reset(Position pos_to_reset, Position new_pos);

    void InvalidOutcoming(Position pos_to_update);
    void InvalidIncoming(Position pos_to_update);
private:
    std::vector<Edge> incoming;
    std::vector<Edge> outcoming;
    std::unordered_map<Position, Edges, PositionHash> incidence_lists;

    ISheet & sheet;
};

#endif //SPREADSHEET_GRAPH_H
