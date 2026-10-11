
#include "network.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>

int Network::addNode(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Node name cannot be empty.");
    }

    for (const auto& existing : names) {
        if (existing == name) {
            throw std::invalid_argument("Node name already exists.");
        }
    }

    names.push_back(name);
    graph.emplace_back();
    online.push_back(true);

    return static_cast<int>(names.size()) - 1;
}

void Network::validateNode(int id) const {
    if (id < 0 || id >= static_cast<int>(names.size())) {
        throw std::out_of_range("Invalid node ID.");
    }
}

bool Network::isNodeOnline(int id) const {
    validateNode(id);
    return online[id];
}

void Network::setNodeOnline(int id, bool status) {
    validateNode(id);
    online[id] = status;
}

void Network::addLink(int from, int to, int cost) {
    validateNode(from);
    validateNode(to);

    if (from == to) {
        throw std::invalid_argument("Self-links are not allowed.");
    }

    if (cost < 0) {
        throw std::invalid_argument("Link cost cannot be negative.");
    }

    for (const auto& edge : graph[from]) {
        if (edge.destination == to) {
            throw std::invalid_argument("Link already exists.");
        }
    }

    graph[from].push_back({to, cost});
    graph[to].push_back({from, cost});
}

void Network::updateLinkCost(int from, int to, int newCost) {
    validateNode(from);
    validateNode(to);

    if (from == to) {
        throw std::invalid_argument("Self-links are not allowed.");
    }

    if (newCost < 0) {
        throw std::invalid_argument("Link cost cannot be negative.");
    }

    Edge* forward = nullptr;
    Edge* backward = nullptr;

    for (auto& edge : graph[from]) {
        if (edge.destination == to) {
            forward = &edge;
            break;
        }
    }

    for (auto& edge : graph[to]) {
        if (edge.destination == from) {
            backward = &edge;
            break;
        }
    }

    if (forward == nullptr || backward == nullptr) {
        throw std::invalid_argument("Link does not exist.");
    }

    forward->cost = newCost;
    backward->cost = newCost;
}

void Network::removeLink(int from, int to) {
    validateNode(from);
    validateNode(to);

    if (from == to) {
        throw std::invalid_argument("Self-links are not allowed.");
    }

    auto& forward = graph[from];
    auto& backward = graph[to];

    auto forwardIt = std::find_if(
        forward.begin(), forward.end(),
        [to](const Edge& edge) {
            return edge.destination == to;
        }
    );

    auto backwardIt = std::find_if(
        backward.begin(), backward.end(),
        [from](const Edge& edge) {
            return edge.destination == from;
        }
    );

    if (forwardIt == forward.end() ||
        backwardIt == backward.end()) {
        throw std::invalid_argument("Link does not exist.");
    }

    forward.erase(forwardIt);
    backward.erase(backwardIt);
}

void Network::printNetwork() const {
    std::cout << "\nNetwork nodes:\n";

    for (std::size_t i = 0; i < names.size(); ++i) {
        std::cout << names[i] << " ("
                  << (online[i] ? "online" : "offline")
                  << ")\n";
    }

    std::cout << "\nNetwork connections:\n";

    for (std::size_t from = 0; from < graph.size(); ++from) {
        for (const auto& edge : graph[from]) {
            if (static_cast<int>(from) < edge.destination) {
                std::cout << names[from] << " <-> "
                          << names[edge.destination]
                          << " (cost: " << edge.cost << ")\n";
            }
        }
    }
}

const std::string& Network::nodeName(int id) const {
    validateNode(id);
    return names[id];
}

int Network::nodeCount() const {
    return static_cast<int>(names.size());
}

RouteResult Network::shortestPath(
    int source,
    int destination
) const {
    validateNode(source);
    validateNode(destination);

    RouteResult result;

    if (!online[source] || !online[destination]) {
        return result;
    }

    const int INF = std::numeric_limits<int>::max();

    std::vector<int> distance(graph.size(), INF);
    std::vector<int> parent(graph.size(), -1);

    using State = std::pair<int, int>;

    std::priority_queue<
        State,
        std::vector<State>,
        std::greater<State>
    > pq;

    distance[source] = 0;
    pq.push({0, source});

    while (!pq.empty()) {
        auto [currentCost, node] = pq.top();
        pq.pop();

        if (currentCost != distance[node]) {
            continue;
        }

        if (node == destination) {
            break;
        }

        for (const Edge& edge : graph[node]) {
            if (!online[edge.destination]) {
                continue;
            }

            if (currentCost > INF - edge.cost) {
                continue;
            }

            const int newCost = currentCost + edge.cost;

            if (newCost < distance[edge.destination]) {
                distance[edge.destination] = newCost;
                parent[edge.destination] = node;
                pq.push({newCost, edge.destination});
            }
        }
    }

    if (distance[destination] == INF) {
        return result;
    }

    result.reachable = true;
    result.totalCost = distance[destination];

    for (int node = destination; node != -1; node = parent[node]) {
        result.path.push_back(node);
    }

    std::reverse(result.path.begin(), result.path.end());

    return result;
}
