#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

int main() {
    int serverSocket, clientSocket;
    char buffer[1000];

    // Create socket
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    // Bind
    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // Listen
    listen(serverSocket, 1);

    cout << "Server waiting...\n";

    // Accept client
    clientSocket = accept(serverSocket, nullptr, nullptr);

    // Receive message
    int n = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    buffer[n] = '\0';

    cout << "Received message: " << buffer << endl;

    // Count characters
    int count = strlen(buffer);

    // Send count
    send(clientSocket, &count, sizeof(count), 0);

    close(clientSocket);
    close(serverSocket);

    return 0;
}