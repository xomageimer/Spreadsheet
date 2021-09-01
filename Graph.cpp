#include "Graph.h"
#include "Engine.h"

#include <algorithm>

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

void DependencyGraph::AddEdge(std::shared_ptr<ICell> par_cell, std::shared_ptr<ICell> child_cell) {
    size_t edge_id = outcoming.size();
    outcoming.push_back({par_cell, child_cell});
    vertexes.at(par_cell).outcoming_ids.push_back(edge_id);

    edge_id = incoming.size();
    incoming.push_back({child_cell, par_cell});
    vertexes.at(child_cell).incoming_ids.push_back(edge_id);

    //TODO проверка на ацикличность!
}

void DependencyGraph::InvalidIncoming(std::shared_ptr<ICell> cell_ptr) {
    auto it = vertexes.find(cell_ptr);
    if (it == vertexes.end())
        return;

    auto formula_it = dynamic_cast<FormulaCell *>(it->first.get());
    if (formula_it){
        formula_it->status = FormulaCell::Status::Invalid;
    }

    for (auto id : it->second.incoming_ids) {
        auto next_cell = vertexes.find(incoming.at(id).to.lock());

        auto formula_cell = dynamic_cast<FormulaCell *>(next_cell->first.get());
        if (formula_cell && formula_cell->status == FormulaCell::Status::Invalid){
            return;
        } else {
            InvalidIncoming(next_cell->first);
        }
    }
}

void DependencyGraph::InvalidOutcoming(std::shared_ptr<ICell> cell_ptr) {
    auto it = vertexes.find(cell_ptr);
    if (it == vertexes.end())
        return;

    auto formula_it = dynamic_cast<FormulaCell *>(it->first.get());
    if (formula_it && formula_it->status == FormulaCell::Status::Invalid) {
        return;
    } else if (formula_it) {
        formula_it->status = FormulaCell::Status::Invalid;
    } else {
        return;
    }

    for (auto id : it->second.outcoming_ids) {
        auto next_cell = vertexes.find(outcoming.at(id).to.lock());

        auto formula_cell = dynamic_cast<FormulaCell *>(next_cell->first.get());
        if (formula_cell){
            InvalidOutcoming(next_cell->first);
        }
    }
}
