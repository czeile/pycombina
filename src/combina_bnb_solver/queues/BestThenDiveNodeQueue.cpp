/*
 * BestThenDiveNodeQueue.cpp
 *
 * This file is part of pycombina.
 *
 * Copyright 2017-2018 Adrian Bürger, Clemens Zeile, Sebastian Sager, Moritz Diehl
 *
 * pycombina is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * pycombina is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with pycombina. If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include <algorithm>
#include <limits>
#include <numeric>

#include "BestThenDiveNodeQueue.hpp"
#include "../CombinaBnBSolver.hpp"
#include "../Node.hpp"


BestThenDiveNodeQueue::BestThenDiveNodeQueue(CombinaBnBSolver* solver)
    : NodeQueue(solver),
      curtop(),
      limbo(),
      store()
{}

BestThenDiveNodeQueue::BestThenDiveNodeQueue(const BestThenDiveNodeQueue& queue)
    : NodeQueue(queue),
      curtop(queue.curtop),
      limbo(queue.limbo),
      store(queue.store)
{}

BestThenDiveNodeQueue::BestThenDiveNodeQueue(BestThenDiveNodeQueue&& queue)
    : NodeQueue(std::forward<NodeQueue>(queue)),
      curtop(std::move(queue.curtop)),
      limbo(std::move(queue.limbo)),
      store(std::move(queue.store))
{}

BestThenDiveNodeQueue::~BestThenDiveNodeQueue()
{}

size_t BestThenDiveNodeQueue::size() const {
    return curtop ? limbo.size() + store.size() + 1 : limbo.size() + store.size();
}

NodePtr BestThenDiveNodeQueue::top() const {
    if(!limbo.empty()) {
        return limbo.back();
    }
    else if(curtop) {
        return curtop;
    }
    else {
        return store.front().second;
    }
}

void BestThenDiveNodeQueue::pop() {
    using namespace std::placeholders;

    if(!limbo.empty()) {
        limbo.pop_back();
    }
    if(curtop) {
        curtop.reset();
    }
    else {
        std::pop_heap(store.begin(), store.end(), std::bind(&BestThenDiveNodeQueue::later_root, this, _1, _2));
        store.pop_back();
    }
}

void BestThenDiveNodeQueue::push(const std::vector<NodePtr>& nodes) {
    using namespace std::placeholders;

    // short-circuit for empty lists
    if(nodes.empty()) {
        return;
    }

    // stash current top if still present
    size_t initial_store_size = store.size();
    if(curtop) {
        store.emplace_back(adjusted_lower_bound(curtop), curtop);
        curtop = nullptr;
    }

    // get some info
    const double glob_ub = solver->get_eta();

    // find preferred diving target
    auto tgt_it = nodes.cend();
    for(auto it = nodes.cbegin(); it != nodes.cend(); ++it) {
        if((*it)->get_lb() < glob_ub && (tgt_it == nodes.cend() || prefer_dive(*it, *tgt_it))) {
            tgt_it = it;
        }
    }

    // install diving target as current top
    if(tgt_it != nodes.cend()) {
        curtop = *tgt_it;
    }

    // stash remaining nodes
    for(auto it = nodes.cbegin(); it != tgt_it; ++it) {
        if((*it)->get_lb() >= glob_ub) {
            limbo.push_back(*it);
        }
        else {
            store.emplace_back(adjusted_lower_bound(*it), *it);
        }
    }
    if(tgt_it != nodes.cend()) {
        for(auto it = std::next(tgt_it); it != nodes.cend(); ++it) {
            if((*it)->get_lb() >= glob_ub) {
                limbo.push_back(*it);
            }
            else {
                store.emplace_back(adjusted_lower_bound(*it), *it);
            }
        }
    }

    // reestablish heap property
    const auto begin = store.begin();
    const auto end = store.end();
    for(auto it = std::next(begin, initial_store_size); it != end; ++it) {
        std::push_heap(begin, it, std::bind(&BestThenDiveNodeQueue::later_root, this, _1, _2));
    }
}

void BestThenDiveNodeQueue::clear() {
    curtop.reset();
    limbo.clear();
    store.clear();
}

double BestThenDiveNodeQueue::adjusted_lower_bound(const NodePtr& node) const {
    const std::vector<double>& dt = solver->get_dt();
    const std::vector<unsigned int>& max_sigma = solver->get_num_max_switches();
    const std::vector<unsigned int>& sigma = node->get_sigma();
    
    auto max_it = max_sigma.cbegin();
    auto cur_it = sigma.cbegin();
    unsigned int min_rem_sigma = 0;
    while(max_it != max_sigma.cend()) {
        min_rem_sigma = std::min(min_rem_sigma, *max_it++ - *cur_it++);
    }

    const double rem_time = std::accumulate(std::next(dt.cbegin(), node->get_depth()), dt.cend(), 0.0);
    return node->get_lb() + rem_time / (3 + 2 * min_rem_sigma);
}

bool BestThenDiveNodeQueue::later_root(const std::pair<double, NodePtr>& lhs, const std::pair<double, NodePtr>& rhs) const {
    if(lhs.first != rhs.first) {
        return rhs.first < lhs.first;
    }
    else {
        return rhs.second->get_max_sigma() < lhs.second->get_max_sigma();
    }
}

bool BestThenDiveNodeQueue::prefer_dive(const NodePtr& lhs, const NodePtr& rhs) const {
    if(lhs->get_lb() != rhs->get_lb()) {
        return lhs->get_lb() < rhs->get_lb();
    }
    else {
        return lhs->get_max_sigma() < rhs->get_max_sigma();
    }
}
