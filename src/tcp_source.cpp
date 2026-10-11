
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

int main() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "Winsock initialization failed\n";
        return 1;
    }

    SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in server{};
    server.sin_family = AF_INET;
    server.sin_port = htons(54002);
    inet_pton(AF_INET, "127.0.0.1", &server.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&server),
                sizeof(server)) == SOCKET_ERROR) {
        std::cerr << "Connection failed. Is the relay running?\n";
        closesocket(sock);
        WSACleanup();
        return 1;
    }

    std::cout << "[Source] Connected to relay at 127.0.0.1:54002\n";
    std::cout << "Type a message, or /quit to exit.\n";

    std::string message;

    while (true) {
        std::cout << "\nYou: ";
        if (!std::getline(std::cin, message) || message == "/quit") {
            break;
        }

        if (message.empty()) {
            continue;
        }

        std::string packet = message + "\n";
        size_t sent = 0;

        while (sent < packet.size()) {
            int count = send(sock, packet.data() + sent,
                             static_cast<int>(packet.size() - sent), 0);
            if (count <= 0) {
                std::cerr << "[Source] Send failed\n";
                closesocket(sock);
                WSACleanup();
                return 1;
            }
            sent += static_cast<size_t>(count);
        }

        std::string response;
        char ch;
        bool received = false;

        while (true) {
            int count = recv(sock, &ch, 1, 0);
            if (count <= 0) {
                break;
            }
            if (ch == '\n') {
                received = true;
                break;
            }
            if (response.size() >= 4096) {
                break;
            }
            response += ch;
        }

        if (!received) {
            std::cerr << "[Source] No complete response received\n";
            break;
        }

        std::cout << "[Source] Destination response: "
                  << response << '\n';
    }

    closesocket(sock);
    WSACleanup();
    std::cout << "[Source] Disconnected\n";
    return 0;
}
