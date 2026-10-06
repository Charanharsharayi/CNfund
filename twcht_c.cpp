#include <iostream>
#include <cstring>
#include <thread>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

void receiveMessages(int socket) {
    char buffer[1000];

    while (true) {
        int n = recv(socket, buffer, 999, 0);

        if (n <= 0)
            break;

        buffer[n] = '\0';

        cout << "\nFriend: " << buffer << endl;

        if (strcmp(buffer, "exit") == 0)
            break;
    }
}

void sendMessages(int socket) {
    char buffer[1000];

    while (true) {
        cout << "You: ";
        cin.getline(buffer, 1000);

        send(socket, buffer, strlen(buffer) + 1, 0);

        if (strcmp(buffer, "exit") == 0)
            break;
    }
}

int main() {

    int clientSocket;

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);

    // CHANGE THIS TO THE SERVER LAPTOP'S IP
    inet_pton(AF_INET, "192.168.1.10", &serverAddr.sin_addr);

    connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    cout << "Connected to server!\n";

    thread t1(receiveMessages, clientSocket);
    thread t2(sendMessages, clientSocket);

    t1.join();
    t2.join();

    close(clientSocket);

    return 0;
}