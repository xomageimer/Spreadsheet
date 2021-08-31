#include "Graph.h"

#include "Engine.h"

EdgeID DependencyGraph::AddEdge(Position parent, Position child) {
    EdgeID in_id = incoming.size();
    incoming.push_back({parent, child});
    incidence_lists[parent].incoming_ids.push_back(in_id);

    EdgeID out_id = outcoming.size();
    outcoming.push_back({child, parent});
    incidence_lists[child].outcoming_ids.push_back(out_id);

    //TODO проверять оба на ацикличность

    return in_id;
}

void DependencyGraph::InvalidOutcoming(Position pos_to_update) {
    if (incidence_lists.find(pos_to_update) == incidence_lists.end())
        return;
    auto * cell = sheet.GetCell(pos_to_update);
    auto * formula = dynamic_cast<FormulaCell *>(cell);
    if (formula)
        formula->status = FormulaCell::Status::Invalid;

    for (auto id : incidence_lists[pos_to_update].outcoming_ids) {
        cell = sheet.GetCell(outcoming[id].to);

        formula = dynamic_cast<FormulaCell *>(cell);
        if (formula && formula->status == FormulaCell::Status::Invalid) {
            break;
        } else if (formula) {
            formula->status = FormulaCell::Status::Invalid;
            InvalidOutcoming(outcoming[id].to);
        }
    }
}

void DependencyGraph::InvalidIncoming(Position pos_to_update) {
    if (incidence_lists.find(pos_to_update) == incidence_lists.end())
        return;
    auto * cell = sheet.GetCell(pos_to_update);
    auto * formula = dynamic_cast<FormulaCell *>(cell);
    if (formula)
        formula->status = FormulaCell::Status::Invalid;

    for (auto id : incidence_lists[pos_to_update].incoming_ids) {
        cell = sheet.GetCell(incoming[id].to);

        formula = dynamic_cast<FormulaCell *>(cell);
        if (formula && formula->status == FormulaCell::Status::Invalid) {
            break;
        } else if (formula) {
            formula->status = FormulaCell::Status::Invalid;
            InvalidIncoming(incoming[id].to);
        }
    }
}

void DependencyGraph::Reset(Position pos_to_reset, Position new_pos) {
    auto it = incidence_lists.extract(incidence_lists.find(pos_to_reset));
    if (it){
        it.key() = new_pos;
        incidence_lists.insert(std::move(it));
    }
}

void DependencyGraph::Delete(Position pos) {
    auto it = incidence_lists.find(pos);
    if (it != incidence_lists.end()){
        incidence_lists.erase(it);
    }
}
