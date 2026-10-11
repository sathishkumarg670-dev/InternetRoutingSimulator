
#include <winsock2.h>
#include <ws2tcpip.h>

#include <array>
#include <future>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>

#pragma comment(lib, "Ws2_32.lib")

constexpr unsigned short PORT = 54000;

void closeSocket(SOCKET socket) {
    if (socket != INVALID_SOCKET) {
        closesocket(socket);
    }
}

bool sendAll(SOCKET socket, const std::string& message) {
    std::size_t sent = 0;

    while (sent < message.size()) {
        int result = send(
            socket,
            message.data() + sent,
            static_cast<int>(message.size() - sent),
            0
        );

        if (result == SOCKET_ERROR || result == 0) {
            return false;
        }

        sent += static_cast<std::size_t>(result);
    }

    return true;
}

bool receiveLine(SOCKET socket, std::string& message) {
    message.clear();

    std::array<char, 1024> buffer{};

    while (message.size() < 4096) {
        int received = recv(
            socket,
            buffer.data(),
            static_cast<int>(buffer.size()),
            0
        );

        if (received <= 0) {
            return false;
        }

        for (int i = 0; i < received; ++i) {
            if (buffer[i] == '\n') {
                return true;
            }

            message.push_back(buffer[i]);

            if (message.size() >= 4096) {
                return false;
            }
        }
    }

    return false;
}

void runServer(std::promise<void> ready) {
    SOCKET listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (listener == INVALID_SOCKET) {
        ready.set_exception(std::make_exception_ptr(
            std::runtime_error("Server socket creation failed.")
        ));
        return;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(PORT);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(
            listener,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)
        ) == SOCKET_ERROR) {
        closeSocket(listener);

        ready.set_exception(std::make_exception_ptr(
            std::runtime_error("Bind failed. The port may already be in use.")
        ));
        return;
    }

    if (listen(listener, SOMAXCONN) == SOCKET_ERROR) {
        closeSocket(listener);

        ready.set_exception(std::make_exception_ptr(
            std::runtime_error("Server listen failed.")
        ));
        return;
    }

    // Tell the client that the server is ready.
    ready.set_value();

    std::cout << "[Server] Listening on port " << PORT << '\n';

    SOCKET client = accept(listener, nullptr, nullptr);
    closeSocket(listener);

    if (client == INVALID_SOCKET) {
        std::cerr << "[Server] Accept failed.\n";
        return;
    }

    std::string message;

    if (receiveLine(client, message)) {
        std::cout << "[Server] Received: " << message << '\n';

        if (sendAll(client, "ACK: Message received\n")) {
            std::cout << "[Server] Acknowledgement sent.\n";
        }
    } else {
        std::cerr << "[Server] Message receive failed.\n";
    }

    shutdown(client, SD_BOTH);
    closeSocket(client);
}

int main() {
    WSADATA data{};

    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        std::cerr << "Winsock initialization failed.\n";
        return 1;
    }

    int exitCode = 0;

    try {
        std::promise<void> ready;
        std::future<void> serverReady = ready.get_future();

        std::thread server(
            runServer,
            std::move(ready)
        );

        try {
            serverReady.get();

            SOCKET client = socket(
                AF_INET,
                SOCK_STREAM,
                IPPROTO_TCP
            );

            if (client == INVALID_SOCKET) {
                throw std::runtime_error(
                    "Client socket creation failed."
                );
            }

            sockaddr_in serverAddress{};
            serverAddress.sin_family = AF_INET;
            serverAddress.sin_port = htons(PORT);

            inet_pton(
                AF_INET,
                "127.0.0.1",
                &serverAddress.sin_addr
            );

            if (connect(
                    client,
                    reinterpret_cast<sockaddr*>(&serverAddress),
                    sizeof(serverAddress)
                ) == SOCKET_ERROR) {
                closeSocket(client);
                throw std::runtime_error("TCP connection failed.");
            }

            std::cout << "[Client] Connected to local server.\n";

            if (!sendAll(client, "Hello from my TCP client!\n")) {
                closeSocket(client);
                throw std::runtime_error("Message send failed.");
            }

            std::cout << "[Client] Message sent.\n";

            std::string response;

            if (receiveLine(client, response)) {
                std::cout << "[Client] Server response: "
                          << response << '\n';
            } else {
                closeSocket(client);
                throw std::runtime_error(
                    "Server acknowledgement was not received."
                );
            }

            shutdown(client, SD_BOTH);
            closeSocket(client);
        } catch (...) {
            if (server.joinable()) {
                server.join();
            }
            throw;
        }

        server.join();
    } catch (const std::exception& error) {
        std::cerr << "Error: " << error.what() << '\n';
        exitCode = 1;
    }

    WSACleanup();
    return exitCode;
}
