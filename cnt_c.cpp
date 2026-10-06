#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

int main() {
    int clientSocket;
    char message[1000];
    int count;

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // Connect
    connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // Input message
    cout << "Enter message: ";
    cin.getline(message, 1000);

    // Send message
    send(clientSocket, message, strlen(message), 0);

    // Receive character count
    recv(clientSocket, &count, sizeof(count), 0);

    cout << "Number of characters: " << count << endl;

    close(clientSocket);

    return 0;
}