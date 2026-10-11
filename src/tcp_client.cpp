
#include <winsock2.h>
#include <ws2tcpip.h>

#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

constexpr unsigned short PORT = 54000;
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
        char character;
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
        std::cerr << "Winsock initialization failed.\n";
        return 1;
    }

    SOCKET client = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (client == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(PORT);

    inet_pton(AF_INET, "127.0.0.1", &serverAddress.sin_addr);

    if (connect(
            client,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ) == SOCKET_ERROR) {
        std::cerr << "Connection failed. Is the server running?\n";
        closesocket(client);
        WSACleanup();
        return 1;
    }

    std::cout << "[Client] Connected to server!\n";
    std::cout << "Type a message and press Enter.\n";
    std::cout << "Type /quit to disconnect.\n\n";

    std::string message;

    while (true) {
        std::cout << "You: ";

        if (!std::getline(std::cin, message)) {
            break;
        }

        if (message == "/quit") {
            break;
        }

        if (message.size() >= MAX_MESSAGE_SIZE) {
            std::cout << "Message too long. Maximum: "
                      << MAX_MESSAGE_SIZE - 1 << " characters.\n";
            continue;
        }

        if (message.empty()) {
            continue;
        }

        if (!sendAll(client, message + "\n")) {
            std::cerr << "Message send failed.\n";
            break;
        }

        std::string response;

        if (!receiveLine(client, response)) {
            std::cerr << "Server disconnected or reply failed.\n";
            break;
        }

        std::cout << "[Server] " << response << "\n\n";
    }

    shutdown(client, SD_BOTH);
    closesocket(client);
    WSACleanup();

    std::cout << "[Client] Disconnected.\n";
    return 0;
}
