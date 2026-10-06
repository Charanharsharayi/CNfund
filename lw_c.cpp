#include <iostream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

int main() {
    int clientSocket;
    char ch;

    // Create socket
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // Connect to server
    connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    // Input
    cout << "Enter a lowercase letter: ";
    cin >> ch;

    // Send character
    send(clientSocket, &ch, sizeof(ch), 0);

    // Receive uppercase character
    recv(clientSocket, &ch, sizeof(ch), 0);

    cout << "Server response: " << ch << endl;

    close(clientSocket);

    return 0;
}