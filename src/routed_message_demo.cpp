
#include "network.hpp"

#include <iostream>
#include <string>

struct Packet {
    int source;
    int destination;
    std::string message;
};

void transmitPacket(const Network& network, const Packet& packet) {
    std::cout << "\n========== ROUTED MESSAGE ==========\n";
    std::cout << "Source      : " << network.nodeName(packet.source) << '\n';
    std::cout << "Destination : " << network.nodeName(packet.destination) << '\n';
    std::cout << "Message     : " << packet.message << '\n';

    RouteResult route = network.shortestPath(
        packet.source, packet.destination
    );

    if (!route.reachable) {
        std::cout << "\n[ERROR] Destination is unreachable.\n";
        std::cout << "====================================\n";
        return;
    }

    std::cout << "\n[ROUTER] Dijkstra selected route: ";

    for (std::size_t i = 0; i < route.path.size(); ++i) {
        if (i > 0) std::cout << " -> ";
        std::cout << network.nodeName(route.path[i]);
    }

    std::cout << "\n[ROUTER] Total route cost: "
              << route.totalCost << '\n';

    std::cout << "\n[FORWARDING]\n";

    for (std::size_t i = 0; i + 1 < route.path.size(); ++i) {
        std::cout << "[Node " << network.nodeName(route.path[i])
                  << "] Forwarding packet to "
                  << network.nodeName(route.path[i + 1]) << '\n';
    }

    std::cout << "\n[Node " << network.nodeName(packet.destination)
              << "] Message delivered: " << packet.message << '\n';
    std::cout << "====================================\n";
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

    Packet packet{A, D, "Hello from A to D!"};

    std::cout << "INTERNET ROUTING SIMULATOR\n";
    std::cout << "\n========== INITIAL TOPOLOGY ==========\n";
    network.printNetwork();

    std::cout << "\n========== TEST 1: INITIAL ROUTE ==========\n";
    transmitPacket(network, packet);

    std::cout << "\n========== TEST 2: INCREASE A-B COST ==========\n";
    std::cout << "[NETWORK] Changing link A-B cost from 2 to 10\n";
    network.updateLinkCost(A, B, 10);
    transmitPacket(network, packet);

    std::cout << "\n========== TEST 3: RESTORE A-B COST ==========\n";
    std::cout << "[NETWORK] Changing link A-B cost from 10 to 2\n";
    network.updateLinkCost(A, B, 2);
    transmitPacket(network, packet);

    std::cout << "\n========== FINAL TOPOLOGY ==========\n";
    network.printNetwork();

    return 0;
}
