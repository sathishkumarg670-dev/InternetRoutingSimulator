
#include "network.hpp"

#include <exception>
#include <iostream>

void printRoute(const Network& network, const RouteResult& route) {
    if (!route.reachable) {
        std::cout << "No route available.\n";
        return;
    }

    for (std::size_t i = 0; i < route.path.size(); ++i) {
        std::cout << network.nodeName(route.path[i]);

        if (i + 1 < route.path.size()) {
            std::cout << " -> ";
        }
    }

    std::cout << "\nTotal route cost: " << route.totalCost << '\n';
}

int main() {
    try {
        std::cout << "====================================\n";
        std::cout << "   INTERNET ROUTING SIMULATOR\n";
        std::cout << "====================================\n";

        Network network;

        int A = network.addNode("A");
        int B = network.addNode("B");
        int C = network.addNode("C");
        int D = network.addNode("D");
        int E = network.addNode("E");

        network.addLink(A, B, 2);
        network.addLink(B, D, 2);
        network.addLink(A, C, 5);
        network.addLink(C, D, 1);

        std::cout << "\nInitial network:";
        network.printNetwork();

        std::cout << "\nShortest route from A to D:\n";
        RouteResult firstRoute = network.shortestPath(A, D);
        printRoute(network, firstRoute);

        std::cout << "\nUpdating link A-B cost from 2 to 10...\n";
        network.updateLinkCost(A, B, 10);

        std::cout << "\nUpdated network:";
        network.printNetwork();

        std::cout << "\nRecalculating shortest route from A to D:\n";
        RouteResult updatedRoute = network.shortestPath(A, D);
        printRoute(network, updatedRoute);

        std::cout << "\nChecking unreachable node E:\n";
        RouteResult unreachable = network.shortestPath(A, E);

        if (!unreachable.reachable) {
            std::cout << "E is unreachable from A.\n";
        }

        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        return 1;
    }
}
