#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>

int main() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(9090);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    connect(sock, (sockaddr*)&serverAddr, sizeof(serverAddr));

    std::string message;
    while (true) {
        std::getline(std::cin, message);
        send(sock, message.c_str(), message.size(), 0);

        char buffer[1024] = {0};
        int bytes = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes > 0) {
            std::cout << "Server: " << std::string(buffer, bytes) << std::endl;
        }
    }

    close(sock);
    return 0;
}
