#include "Graph.h"
#include "Engine.h"

#include <algorithm>
#include <utility>

std::weak_ptr<ICell> DependencyGraph::AddVertex(std::shared_ptr<ICell> new_cell) {
    auto [it, b] = vertexes.emplace(std::move(new_cell), Edges{});
    return it->first;
}

// TODO крч тут нужно пройтись по всем ячейкам кт участвуют в формуле cell_ptr
//  и уничтожить их связь с этим удаленным cell_ptr (идея 10/10)
void DependencyGraph::Delete(const std::shared_ptr<ICell>& cell_ptr) {
    auto it = vertexes.find(cell_ptr);
    for (auto & el : it->second.incoming_ids) {
        auto & out_from_child = vertexes.find(incoming.at(el).to.lock())->second.outcoming_ids;
        auto to_del = std::find(out_from_child.begin(), out_from_child.end(), [&](auto id) {
            return outcoming[id].to == it->first;
        });
        outcoming.erase(outcoming.begin() + *to_del);
        out_from_child.erase(to_del);
    }
    vertexes.erase(cell_ptr);
}

EdgeID DependencyGraph::AddEdge(std::shared_ptr<ICell> par_cell, std::shared_ptr<ICell> child_cell) {
    size_t edge_id = outcoming.size();

}
