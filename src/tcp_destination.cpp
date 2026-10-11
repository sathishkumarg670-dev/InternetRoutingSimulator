
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>

#pragma comment(lib, "Ws2_32.lib")

constexpr unsigned short PORT = 54003;

int main() {
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }

    SOCKET server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == INVALID_SOCKET) {
        std::cerr << "Socket creation failed\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(server, reinterpret_cast<sockaddr*>(&address),
             sizeof(address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed. Is port 54003 already in use?\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    if (listen(server, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed\n";
        closesocket(server);
        WSACleanup();
        return 1;
    }

    std::cout << "[Destination] Listening on 127.0.0.1:"
              << PORT << '\n';

    while (true) {
        SOCKET client = accept(server, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            std::cerr << "Accept failed\n";
            continue;
        }

        std::string message;
        char ch;
        while (recv(client, &ch, 1, 0) > 0 && ch != '\n') {
            if (message.size() >= 4096) break;
            message += ch;
        }

        if (!message.empty()) {
            std::cout << "[Destination] Received: "
                      << message << '\n';

            std::string reply = "DELIVERED: " + message + "\n";
            send(client, reply.c_str(),
                 static_cast<int>(reply.size()), 0);
        }

        closesocket(client);
    }

    closesocket(server);
    WSACleanup();
    return 0;
}
