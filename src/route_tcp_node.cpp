#include "network.hpp"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <unordered_map>
#include <limits>
#pragma comment(lib, "Ws2_32.lib")
constexpr unsigned short ROUTER_B_PORT = 54004;
constexpr unsigned short ROUTER_C_PORT = 54005;
constexpr unsigned short DESTINATION_PORT = 54003;
constexpr int MAX_LINE = 4096;
struct NodeAddress {
    const char* host;
    unsigned short port;
};
const std::unordered_map<std::string, NodeAddress> nodeAddresses = {
    {"B", {"127.0.0.1", ROUTER_B_PORT}},
    {"C", {"127.0.0.1", ROUTER_C_PORT}},
    {"D", {"127.0.0.1", DESTINATION_PORT}}
};
const std::unordered_map<std::string, int> nodeIndices = {
    {"A", 0}, {"B", 1}, {"C", 2}, {"D", 3}
};
bool sendAll(SOCKET socketHandle, const std::string& data) {
    std::size_t sent = 0;
    while (sent < data.size()) {
        int count = send(
            socketHandle,
            data.data() + sent,
            static_cast<int>(data.size() - sent),
            0
        );
        if (count == SOCKET_ERROR || count == 0) {
            return false;
        }
        sent += static_cast<std::size_t>(count);
    }
    return true;
}
bool receiveLine(SOCKET socketHandle, std::string& line) {
    line.clear();
    while (line.size() < MAX_LINE) {
        char ch = '\0';
        int count = recv(socketHandle, &ch, 1, 0);
        if (count <= 0) {
            return false;
        }
        if (ch == '\n') {
            return true;
        }
        if (ch != '\r') {
            line += ch;
        }
    }
    return false;
}
bool connectTo(
    const char* host,
    unsigned short port,
    SOCKET& connection
) {
    connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection == INVALID_SOCKET) {
        return false;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &address.sin_addr) != 1 ||
        connect(
            connection,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)
        ) == SOCKET_ERROR) {
        closesocket(connection);
        connection = INVALID_SOCKET;
        return false;
    }
    return true;
}
// All nodes use the same weighted network.
Network createNetwork() {
    Network network;
    int A = network.addNode("A");
    int B = network.addNode("B");
    int C = network.addNode("C");
    int D = network.addNode("D");
    network.addLink(A, B, 2);
    network.addLink(B, D, 10);
    network.addLink(A, C, 5);
    network.addLink(C, D, 1);
    network.addLink(B, C, 3);
    return network;
}
void printRoute(const Network& network, const RouteResult& route) {
    for (std::size_t i = 0; i < route.path.size(); ++i) {
        if (i > 0) {
            std::cout << " -> ";
        }
        std::cout << network.nodeName(route.path[i]);
    }
    std::cout << " (cost " << route.totalCost << ")\n";
}
// Forward to the next hop selected by Dijkstra.
bool forwardToNextHop(
    const Network& network,
    const RouteResult& route,
    const std::string& destination,
    const std::string& message,
    std::string& response
) {
    if (!route.reachable || route.path.size() < 2) {
        response = "ERROR: no reachable next hop";
        return false;
    }
    const std::string& nextNode = network.nodeName(route.path[1]);
    auto address = nodeAddresses.find(nextNode);
    if (address == nodeAddresses.end()) {
        response = "ERROR: no TCP address for node " + nextNode;
        return false;
    }
    std::cout << "[Forwarding] " << nextNode << " at "
              << address->second.host << ':'
              << address->second.port << '\n';
    SOCKET nextSocket = INVALID_SOCKET;
    if (!connectTo(
            address->second.host,
            address->second.port,
            nextSocket)) {
        response = "ERROR: cannot connect to next hop " + nextNode;
        return false;
    }
    bool sent = sendAll(
        nextSocket,
        destination + "|" + message + "\n"
    );
    bool received = sent && receiveLine(nextSocket, response);
    shutdown(nextSocket, SD_BOTH);
    closesocket(nextSocket);
    if (!sent) {
        response = "ERROR: failed to send to node " + nextNode;
        return false;
    }
    if (!received) {
        response = "ERROR: no response from node " + nextNode;
        return false;
    }
    std::cout << "[Forwarding] Response received from "
              << nextNode << '\n';
    return true;
}
int runNode(const std::string& nodeName, unsigned short listenPort) {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[Node " << nodeName
                  << "] Winsock initialization failed\n";
        return 1;
    }
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) {
        std::cerr << "[Node " << nodeName
                  << "] Socket creation failed\n";
        WSACleanup();
        return 1;
    }
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(listenPort);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(
            listener,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "[Node " << nodeName
                  << "] Bind failed on port " << listenPort
                  << ". Stop the old node process first.\n";
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[Node " << nodeName << "] Listen failed\n";
        closesocket(listener);
        WSACleanup();
        return 1;
    }
    std::cout << "[Node " << nodeName
              << "] Listening on 127.0.0.1:"
              << listenPort << '\n';
    Network network = createNetwork();
    const int currentIndex = nodeIndices.at(nodeName);
    while (true) {
        SOCKET incoming = accept(listener, nullptr, nullptr);
        if (incoming == INVALID_SOCKET) {
            std::cerr << "[Node " << nodeName << "] Accept failed\n";
            continue;
        }
        std::string packet;
        if (!receiveLine(incoming, packet)) {
            std::cerr << "[Node " << nodeName
                      << "] Failed to receive packet\n";
            shutdown(incoming, SD_BOTH);
            closesocket(incoming);
            continue;
        }
        const std::size_t separator = packet.find('|');
        if (separator == std::string::npos) {
            sendAll(incoming, "ERROR: expected destination|message\n");
        } else {
            const std::string destination = packet.substr(0, separator);
            const std::string message = packet.substr(separator + 1);
            std::cout << "\n[Node " << nodeName
                      << "] Received packet for " << destination
                      << ": " << message << '\n';
            if (nodeAddresses.find(destination) == nodeAddresses.end()) {
                sendAll(incoming, "ERROR: destination must be B, C or D\n");
            } else if (nodeName == destination) {
                std::cout << "[Destination " << nodeName
                          << "] Delivered message: " << message << '\n';
                sendAll(
                    incoming,
                    "DELIVERED to " + destination + ": " + message + "\n"
                );
            } else {
                const int targetIndex = nodeIndices.at(destination);
                RouteResult route =
                    network.shortestPath(currentIndex, targetIndex);
                std::cout << "[Node " << nodeName
                          << "] Shortest route to " << destination << ": ";
                if (route.reachable) {
                    printRoute(network, route);
                } else {
                    std::cout << "unreachable\n";
                }
                std::string response;
                if (forwardToNextHop(
                        network,
                        route,
                        destination,
                        message,
                        response)) {
                    sendAll(incoming, response + "\n");
                } else {
                    if (response.empty()) {
                        response = "ERROR: forwarding failed";
                    }
                    sendAll(incoming, response + "\n");
                }
            }
        }
        shutdown(incoming, SD_BOTH);
        closesocket(incoming);
    }
    // The node loop runs until the process is stopped.
    // Startup failures above clean up their resources.
}
int runSource() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "[Source A] Winsock initialization failed\n";
        return 1;
    }
    SOCKET connection = INVALID_SOCKET;
    if (!connectTo("127.0.0.1", ROUTER_B_PORT, connection)) {
        std::cerr << "[Source A] Cannot connect to node B\n";
        WSACleanup();
        return 1;
    }
    std::cout << "[Source A] Connected to node B\n";
    std::cout << "Choose destination B, C or D. Type /quit to exit.\n";
    while (true) {
        std::string destination;
        std::string message;
        std::cout << "\nDestination (B/C/D): ";
        if (!std::getline(std::cin, destination) || destination == "/quit") {
            break;
        }
        if (nodeAddresses.find(destination) == nodeAddresses.end()) {
            std::cout << "Invalid destination. Choose B, C or D.\n";
            continue;
        }
        std::cout << "Message: ";
        if (!std::getline(std::cin, message) || message == "/quit") {
            break;
        }
        if (message.empty() || message.find('|') != std::string::npos) {
            std::cout << "Message must be non-empty and cannot contain |.\n";
            continue;
        }
        if (!sendAll(connection, destination + "|" + message + "\n")) {
            std::cerr << "[Source A] Failed to send message\n";
            break;
        }
        std::string response;
        if (!receiveLine(connection, response)) {
            std::cerr << "[Source A] No response received\n";
            break;
        }
        std::cout << "[Source A] " << response << '\n';
    }
    shutdown(connection, SD_BOTH);
    closesocket(connection);
    WSACleanup();
    return 0;
}
int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout
            << "Usage:\n"
            << "  route_tcp_node source\n"
            << "  route_tcp_node B\n"
            << "  route_tcp_node C\n"
            << "  route_tcp_node D\n";
        return 1;
    }
    const std::string mode = argv[1];
    if (mode == "source") {
        return runSource();
    }
    if (mode == "B") {
        return runNode("B", ROUTER_B_PORT);
    }
    if (mode == "C") {
        return runNode("C", ROUTER_C_PORT);
    }
    if (mode == "D") {
        return runNode("D", DESTINATION_PORT);
    }
    std::cerr << "Unknown mode: " << mode << '\n';
    return 1;
}
