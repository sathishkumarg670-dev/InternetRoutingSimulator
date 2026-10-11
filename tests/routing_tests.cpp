
#include "network.hpp"

#include <cassert>
#include <iostream>
#include <stdexcept>
#include <vector>

void testShortestPath() {
    Network network;

    int A = network.addNode("A");
    int B = network.addNode("B");
    int C = network.addNode("C");
    int D = network.addNode("D");

    network.addLink(A, B, 2);
    network.addLink(B, D, 2);
    network.addLink(A, C, 5);
    network.addLink(C, D, 1);

    RouteResult route = network.shortestPath(A, D);

    assert(route.reachable);
    assert(route.totalCost == 4);
    assert((route.path == std::vector<int>{A, B, D}));
}

void testUnreachableNode() {
    Network network;

    int A = network.addNode("A");
    int B = network.addNode("B");

    RouteResult route = network.shortestPath(A, B);

    assert(!route.reachable);
}

void testNegativeLinkRejected() {
    Network network;

    int A = network.addNode("A");
    int B = network.addNode("B");

    bool rejected = false;

    try {
        network.addLink(A, B, -1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
}

void testLinkCostUpdateChangesRoute() {
    Network network;

    int A = network.addNode("A");
    int B = network.addNode("B");
    int C = network.addNode("C");
    int D = network.addNode("D");

    network.addLink(A, B, 2);
    network.addLink(B, D, 2);
    network.addLink(A, C, 5);
    network.addLink(C, D, 1);

    RouteResult before = network.shortestPath(A, D);

    assert(before.reachable);
    assert(before.totalCost == 4);

    network.updateLinkCost(A, B, 10);

    RouteResult after = network.shortestPath(A, D);

    assert(after.reachable);
    assert(after.totalCost == 6);
    assert((after.path == std::vector<int>{A, C, D}));
}

void testDuplicateLinkRejected() {
    Network network;

    int A = network.addNode("A");
    int B = network.addNode("B");

    network.addLink(A, B, 2);

    bool rejected = false;

    try {
        network.addLink(A, B, 3);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }

    assert(rejected);
}

void testOfflineRelayUsesAlternateRoute() {
    Network network;

    int A = network.addNode("A");
    int B = network.addNode("B");
    int C = network.addNode("C");
    int D = network.addNode("D");

    network.addLink(A, B, 1);
    network.addLink(B, D, 1);
    network.addLink(A, C, 3);
    network.addLink(C, D, 3);

    network.setNodeOnline(B, false);

    RouteResult route = network.shortestPath(A, D);

    assert(route.reachable);
    assert(route.totalCost == 6);
    assert((route.path == std::vector<int>{A, C, D}));
}

void testOfflineRelayMakesDestinationUnreachable() {
    Network network;

    int A = network.addNode("A");
    int B = network.addNode("B");
    int C = network.addNode("C");

    network.addLink(A, B, 1);
    network.addLink(B, C, 1);

    network.setNodeOnline(B, false);

    RouteResult route = network.shortestPath(A, C);

    assert(!route.reachable);
}

int main() {
    testShortestPath();
    testUnreachableNode();
    testNegativeLinkRejected();
    testLinkCostUpdateChangesRoute();
    testDuplicateLinkRejected();
    testOfflineRelayUsesAlternateRoute();
    testOfflineRelayMakesDestinationUnreachable();

    std::cout << "All 7 routing tests passed!\n";

    return 0;
}
