
#include "network.hpp"

#include <iostream>
#include <string>

void sendRoutedMessage(
    const Network& network,
    int source,
    int destination,
    const std::string& message
) {
    std::cout << "\n[Source] " << network.nodeName(source)
              << " wants to send: " << message << '\n';

    RouteResult route = network.shortestPath(source, destination);

    if (!route.reachable) {
        std::cout << "[Router] No available route to "
                  << network.nodeName(destination) << '\n';
        return;
    }

    std::cout << "[Router] Selected route: ";

    for (std::size_t i = 0; i < route.path.size(); ++i) {
        if (i != 0) {
            std::cout << " -> ";
        }

        std::cout << network.nodeName(route.path[i]);
    }

    std::cout << "\n[Router] Total cost: "
              << route.totalCost << '\n';

    for (std::size_t i = 0; i + 1 < route.path.size(); ++i) {
        int current = route.path[i];
        int next = route.path[i + 1];

        std::cout << "[Forwarding] "
                  << network.nodeName(current)
                  << " -> " << network.nodeName(next) << '\n';
    }

    std::cout << "[Destination] Message delivered: "
              << message << '\n';
}

int main() {
    Network network;

    int A = network.addNode("A");
    int B = network.addNode("B");
    int C = network.addNode("C");
    int D = network.addNode("D");

    network.addLink(A, B, 2);
    network.addLink(B, D, 2);
    network.addLink(A, C, 5);
    network.addLink(C, D, 1);
    network.addLink(B, C, 3);

    std::cout << "ROUTE-AWARE FORWARDING DEMO\n";

    sendRoutedMessage(network, A, D, "Hello through the best route!");

    std::cout << "\n[Network] Increasing link A-B cost to 10\n";
    network.updateLinkCost(A, B, 10);

    sendRoutedMessage(network, A, D, "Testing the alternate route!");

    return 0;
}
