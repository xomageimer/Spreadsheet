#include "Graph.h"
#include "Engine.h"

#include <algorithm>

std::weak_ptr<DefaultCell> DependencyGraph::AddVertex(Position pos, std::shared_ptr<DefaultCell> new_cell) {
    if (IsExist(pos)) {
        *cache_cells_located_behind_table[pos].cur_val = std::move(*new_cell);
        auto it = vertexes.find(cache_cells_located_behind_table[pos].cur_val);
        Delete(pos);
        return it->first;
    } else {
        vertexes[new_cell] = Edges{};
        return vertexes.find(new_cell)->first;
    }
}

void DependencyGraph::Delete(Position pos, const std::shared_ptr<DefaultCell>& cell_ptr) {
    auto it = vertexes.find(cell_ptr);
    auto inc_ids = it->second.incoming_ids;
    for (auto el : it->second.incoming_ids) {
        auto child = vertexes.find(incoming.at(el).to.lock());
        auto & out_from_child = child->second.outcoming_ids;
        auto to_del = std::find_if(out_from_child.begin(), out_from_child.end(), [&](auto id) {
            return outcoming[id].to.lock() == it->first;
        });
        assert(to_del != out_from_child.end());

        outcoming.erase(*to_del);
        out_from_child.erase(to_del);
        incoming.erase(el);
    }
    it->second.incoming_ids.clear();
    auto child_cells = cell_ptr->GetReferencedCells();
    for (auto & child : child_cells) {
        if (auto child_it = cache_cells_located_behind_table.find(child); child_it != cache_cells_located_behind_table.end()) {
            if (child_it->second.cur_val && vertexes.at(child_it->second.cur_val).outcoming_ids.empty()) {
                vertexes.erase(child_it->second.cur_val);
                Delete(child);
            } else if (vertexes.at(child_it->second.cur_val).outcoming_ids.empty()) {
                Delete(child);
            }
        }
    }
    if (vertexes.at(cell_ptr).outcoming_ids.empty()) {
        vertexes.erase(cell_ptr);
    } else {
        *cell_ptr = DefaultCell("");
        cache_cells_located_behind_table.emplace(pos, CacheVertex{cell_ptr});
    }
}

void DependencyGraph::AddEdge(Position par_pos, Position child_pos) {
    if (outcoming.empty() && incoming.empty())
        c = 0;
    if (c == INT_MAX)
        throw std::logic_error("table memory exceeded!");

    auto spread_sheet = dynamic_cast<SpreadSheet * const>(&sheet);
    if (!spread_sheet)
        throw std::bad_cast();

    auto par_cell_weak = spread_sheet->cells[par_pos.row][par_pos.col];
    std::weak_ptr<DefaultCell> child_cell_weak;

    if (!vertexes.count(par_cell_weak.lock()))
        throw std::logic_error("can't find parent cell");
    auto par_cell = par_cell_weak.lock();

    if (!(spread_sheet->size > child_pos) || (static_cast<int>(spread_sheet->cells[child_pos.row].size()) <= child_pos.col) || (spread_sheet->cells[child_pos.row][child_pos.col].expired() && !IsExist(child_pos))){
        auto it = cache_cells_located_behind_table.emplace(child_pos, CacheVertex{std::make_shared<DefaultCell>("")});
        vertexes.emplace(it.first->second.cur_val, Edges{});
    } else {
        child_cell_weak = spread_sheet->cells[child_pos.row][child_pos.col];
    }
    auto child_cell = (!child_cell_weak.expired()) ? vertexes.find(child_cell_weak.lock())->first : cache_cells_located_behind_table.at(child_pos).cur_val;

    size_t edge_id = c;
    auto out_it = outcoming.emplace(edge_id, Edge{child_cell, par_cell});
    vertexes.at(child_cell).outcoming_ids.push_back(edge_id);

    edge_id = c++;
    auto in_it = incoming.emplace(edge_id, Edge{par_cell, child_cell});
    vertexes.at(par_cell).incoming_ids.push_back(edge_id);

    try {
        CheckAcyclicity(par_cell);
    } catch (const CircularDependencyException& excp) {
        outcoming.erase(out_it.first);
        incoming.erase(in_it.first);
        vertexes.at(par_cell).incoming_ids.pop_back();
        vertexes.at(child_cell).outcoming_ids.pop_back();
        throw excp;
    }
}

void DependencyGraph::InvalidIncoming(std::shared_ptr<DefaultCell> cell_ptr) {
    auto it = vertexes.find(cell_ptr);
    if (it == vertexes.end())
        return;

    auto formula_it = it->first->GetFormula().get();
    if (formula_it){
        formula_it->status = DefaultFormula::Status::Invalid;
    } else {
        return;
    }

    for (auto id : it->second.incoming_ids) {
        auto next_cell = vertexes.find(incoming.at(id).to.lock());

        auto formula_cell = it->first->GetFormula().get();
        if (formula_cell && formula_cell->status == DefaultFormula::Status::Invalid){
            return;
        } else if (formula_cell) {
            InvalidIncoming(next_cell->first);
        }
    }
}

void DependencyGraph::InvalidOutcoming(std::shared_ptr<DefaultCell> cell_ptr) {
    auto it = vertexes.find(cell_ptr);
    if (it == vertexes.end())
        return;

    auto formula_it = it->first->GetFormula().get();
    if (formula_it && formula_it->status == DefaultFormula::Status::Invalid) {
        return;
    } else if (formula_it) {
        formula_it->status = DefaultFormula::Status::Invalid;
    }

    for (auto id : it->second.outcoming_ids) {
        auto next_cell = vertexes.find(outcoming.at(id).to.lock());

        InvalidOutcoming(next_cell->first);
    }
}

bool DependencyGraph::IsExist(Position pos) {
    if (cache_cells_located_behind_table.find(pos) != cache_cells_located_behind_table.end())
        return true;
    return false;
}

void DependencyGraph::Delete(Position pos) {
    cache_cells_located_behind_table.erase(pos);
}

void DependencyGraph::InvalidOutcoming(Position pos) {
    if (cache_cells_located_behind_table.count(pos))
        InvalidOutcoming(cache_cells_located_behind_table.at(pos).cur_val);
}

void DependencyGraph::InsertRows(int before, int count) {
    if (cache_cells_located_behind_table.empty())
        return;
    if (std::prev(cache_cells_located_behind_table.end())->first.row >= Position::kMaxRows - count)
        throw TableTooBigException("can't insert because too big exception");

    std::map<Position, CacheVertex> new_cache;
    for (auto & el : cache_cells_located_behind_table){
        if (el.first.row >= before) {
            new_cache[{el.first.row + count, el.first.col}] = std::move(el.second);
        } else {
            new_cache[el.first] = std::move(el.second);
        }
    }
    std::swap(cache_cells_located_behind_table, new_cache);
}

void DependencyGraph::InsertCols(int before, int count) {
    if (cache_cells_located_behind_table.empty())
        return;
    if (std::prev(cache_cells_located_behind_table.end())->first.col >= Position::kMaxCols - count)
        throw TableTooBigException("can't insert because too big exception");

    std::map<Position, CacheVertex> new_cache;
    for (auto & el : cache_cells_located_behind_table){
        if (el.first.col >= before) {
            new_cache[{el.first.row, el.first.col + count}] = std::move(el.second);
        } else {
            new_cache[el.first] = std::move(el.second);
        }
    }
    std::swap(cache_cells_located_behind_table, new_cache);
}

void DependencyGraph::DeleteRows(int first, int count) {
    std::map<Position, CacheVertex> new_cache;
    for (auto & el : cache_cells_located_behind_table){
        if (el.first.row > first && el.first.row >= first + count) {
            new_cache[el.first] = std::move(el.second);
        } else {
            vertexes.erase(new_cache[el.first].cur_val);
        }
    }
    std::swap(cache_cells_located_behind_table, new_cache);
}

void DependencyGraph::DeleteCols(int first, int count) {
    std::map<Position, CacheVertex> new_cache;
    for (auto & el : cache_cells_located_behind_table){
        if (el.first.col >= first && el.first.col >= first + count) {
            new_cache[el.first] = std::move(el.second);
        } else {
            vertexes.erase(new_cache[el.first].cur_val);
        }
    }
    std::swap(cache_cells_located_behind_table, new_cache);
}

void DependencyGraph::CheckAcyclicity(std::shared_ptr<struct DefaultCell> cell_ptr) {
    std::unordered_set<std::shared_ptr<struct DefaultCell>> solo_cells;

    solo_cells.insert(cell_ptr);
    CheckAcylicityImpl(cell_ptr, solo_cells);
}

void DependencyGraph::CheckAcylicityImpl(std::shared_ptr<struct DefaultCell> cell_ptr,
                                         std::unordered_set<std::shared_ptr<struct DefaultCell>> &cols) {
    for (int id : vertexes.at(cell_ptr).outcoming_ids){
        if (cols.count(outcoming.at(id).to.lock()))
            throw CircularDependencyException{"circular dependency"};
        cols.insert(outcoming.at(id).to.lock());
        CheckAcylicityImpl(outcoming.at(id).to.lock(), cols);
    }
}

Position DependencyGraph::GetMaxCachePos() const {
    if (cache_cells_located_behind_table.empty())
        return {0, 0};
    return std::prev(cache_cells_located_behind_table.end())->first;
}

bool DependencyGraph::HasOutcomings(Position pos) {
    if (!IsExist(pos))
        return false;
    if (!vertexes.at(cache_cells_located_behind_table.at(pos).cur_val).outcoming_ids.empty())
        return true;
    else return false;
}
