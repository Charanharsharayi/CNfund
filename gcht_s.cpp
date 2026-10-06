#include <iostream>
#include <vector>
#include <thread>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

vector<int> clients;

void handleClient(int clientSocket) {
    char buffer[1000];

    while (true) {
        int n = recv(clientSocket, buffer, 999, 0);

        if (n <= 0)
            break;

        buffer[n] = '\0';

        cout << "Message: " << buffer << endl;

        // Send message to all clients
        for (int client : clients) {
            if (client != clientSocket) {
                send(client, buffer, strlen(buffer) + 1, 0);
            }
        }
    }

    close(clientSocket);
}

int main() {

    int serverSocket;

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket,
         (sockaddr*)&serverAddr,
         sizeof(serverAddr));

    listen(serverSocket, 10);

    cout << "Server started...\n";

    while (true) {

        int clientSocket = accept(
            serverSocket,
            nullptr,
            nullptr
        );

        cout << "New client connected\n";

        clients.push_back(clientSocket);

        thread t(handleClient, clientSocket);
        t.detach();
    }

    close(serverSocket);

    return 0;
}