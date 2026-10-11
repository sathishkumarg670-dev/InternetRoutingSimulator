
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

constexpr unsigned short PORT = 54000;
constexpr std::size_t MAX_MESSAGE_SIZE = 4096;

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

int main() {
    WSADATA data{};

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "[Error] Winsock initialization failed.\n";
        return 1;
    }

    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener == INVALID_SOCKET) {
        std::cerr << "[Error] Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);

    // Listen on available IPv4 network interfaces.
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
            listener,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)
        ) == SOCKET_ERROR) {
        std::cerr << "[Error] Bind failed. Port "
                  << PORT << " may already be in use.\n";

        closesocket(listener);
        WSACleanup();
        return 1;
    }

    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "[Error] Listen failed.\n";

        closesocket(listener);
        WSACleanup();
        return 1;
    }

    std::cout << "[Server] Listening on port " << PORT << '\n';
    std::cout << "[Server] Waiting for clients...\n";
    std::cout << "[Server] Press Ctrl+C to stop.\n";

    // Accept clients sequentially and handle multiple messages.
    while (true) {
        SOCKET client = accept(listener, nullptr, nullptr);

        if (client == INVALID_SOCKET) {
            std::cerr << "[Error] Accept failed.\n";
            break;
        }

        std::cout << "\n[Server] Client connected.\n";

        std::string message;

        while (receiveLine(client, message)) {
            std::cout << "[Client] " << message << '\n';

            std::string response = "ACK: " + message + "\n";

            if (!sendAll(client, response)) {
                std::cerr << "[Error] Failed to send acknowledgement.\n";
                break;
            }

            std::cout << "[Server] Acknowledgement sent.\n";
        }

        std::cout << "[Server] Client disconnected.\n";

        shutdown(client, SD_BOTH);
        closesocket(client);
    }

    closesocket(listener);
    WSACleanup();

    return 0;
}
