
#pragma once

#include <string>
#include <vector>

struct Edge {
    int destination;
    int cost;
};

struct RouteResult {
    bool reachable = false;
    int totalCost = 0;
    std::vector<int> path;
};

class Network {
public:
    int addNode(const std::string& name);

    void addLink(int from, int to, int cost);
    void updateLinkCost(int from, int to, int newCost);
    void removeLink(int from, int to);

    void setNodeOnline(int id, bool online);
    bool isNodeOnline(int id) const;

    RouteResult shortestPath(int source, int destination) const;

    const std::string& nodeName(int id) const;
    int nodeCount() const;

    void printNetwork() const;

private:
    std::vector<std::string> names;
    std::vector<std::vector<Edge>> graph;
    std::vector<bool> online;

    void validateNode(int id) const;
};
