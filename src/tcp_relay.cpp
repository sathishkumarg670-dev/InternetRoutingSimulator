
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

constexpr int MAX_MESSAGE_SIZE = 4096;

bool sendAll(SOCKET socket, const std::string& message) {
    std::size_t sent = 0;

    while (sent < message.size()) {
        int result = send(
            socket,
            message.data() + sent,
            static_cast<int>(message.size() - sent),
            0
        );

        if (result <= 0) {
            return false;
        }

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

bool receiveLine(SOCKET socket, std::string& message) {
    message.clear();

    while (message.size() < MAX_MESSAGE_SIZE) {
        char character = '\0';
        int received = recv(socket, &character, 1, 0);

        if (received <= 0) {
            return false;
        }

        if (character == '\n') {
            return true;
        }

        if (character != '\r') {
            message.push_back(character);
        }
    }

    return false;
}

bool parsePort(const char* text, unsigned short& port) {
    try {
        std::string value(text);
        std::size_t used = 0;
        unsigned long number = std::stoul(value, &used);

        if (used != value.size() || number == 0 || number > 65535) {
            return false;
        }

        port = static_cast<unsigned short>(number);
        return true;
    } catch (...) {
        return false;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 4) {
        std::cerr
            << "Usage: tcp_relay <listen_port> <next_host> <next_port>\n"
            << "Example: tcp_relay 54002 127.0.0.1 54003\n";
        return 1;
    }

    unsigned short listenPort = 0;
    unsigned short nextPort = 0;

    if (!parsePort(argv[1], listenPort) ||
        !parsePort(argv[3], nextPort)) {
        std::cerr << "[Error] Ports must be between 1 and 65535.\n";
        return 1;
    }

    WSADATA data{};

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "[Error] Winsock initialization failed.\n";
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener == INVALID_SOCKET) {
        std::cerr << "[Error] Listener socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in localAddress{};
    localAddress.sin_family = AF_INET;
    localAddress.sin_port = htons(listenPort);
    localAddress.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(
            listener,
            reinterpret_cast<sockaddr*>(&localAddress),
            sizeof(localAddress)
        ) == SOCKET_ERROR ||
        listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[Error] Cannot listen on port "
                  << listenPort << ".\n";
        closesocket(listener);
        WSACleanup();
        return 1;
    }

    std::cout << "[Relay] Listening on 127.0.0.1:"
              << listenPort << '\n';
    std::cout << "[Relay] Next hop: "
              << argv[2] << ':' << nextPort << '\n';

    while (true) {
        SOCKET incoming = accept(listener, nullptr, nullptr);

        if (incoming == INVALID_SOCKET) {
            std::cerr << "[Relay] Accept failed.\n";
            break;
        }

        std::string packet;

        while (receiveLine(incoming, packet)) {
            std::cout << "[Relay] Received packet: "
                      << packet << '\n';

            SOCKET outgoing = socket(
                AF_INET, SOCK_STREAM, IPPROTO_TCP
            );

            if (outgoing == INVALID_SOCKET) {
                std::cerr << "[Relay] Outgoing socket failed.\n";
                break;
            }

            sockaddr_in nextAddress{};
            nextAddress.sin_family = AF_INET;
            nextAddress.sin_port = htons(nextPort);

            if (inet_pton(
                    AF_INET, argv[2], &nextAddress.sin_addr
                ) != 1 ||
                connect(
                    outgoing,
                    reinterpret_cast<sockaddr*>(&nextAddress),
                    sizeof(nextAddress)
                ) == SOCKET_ERROR) {
                std::cerr << "[Relay] Cannot connect to next hop.\n";
                closesocket(outgoing);
                break;
            }

            // Forward the packet to the configured next node.
            bool sent = sendAll(outgoing, packet + "\n");
            std::string response;
            bool received = sent && receiveLine(outgoing, response);

            shutdown(outgoing, SD_BOTH);
            closesocket(outgoing);

            if (!received) {
                std::cerr << "[Relay] Forwarding failed.\n";
                break;
            }

            std::cout << "[Relay] Next hop replied: "
                      << response << '\n';

            if (!sendAll(incoming, response + "\n")) {
                std::cerr << "[Relay] Reply to sender failed.\n";
                break;
            }
        }

        shutdown(incoming, SD_BOTH);
        closesocket(incoming);
        std::cout << "[Relay] Incoming connection closed.\n";
    }

    closesocket(listener);
    WSACleanup();
    return 0;
}
