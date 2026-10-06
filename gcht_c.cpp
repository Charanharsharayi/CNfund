#include <iostream>
#include <thread>
#include <cstring>
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

        cout << "\nMessage: " << buffer << endl;
        cout << "You: ";
    }
}

int main() {

    int clientSocket;

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);

    // Change this to server's IP address
    inet_pton(AF_INET,
              "192.168.1.10",
              &serverAddr.sin_addr);

    connect(clientSocket,
            (sockaddr*)&serverAddr,
            sizeof(serverAddr));

    cout << "Connected to server!\n";

    // Thread for receiving messages
    thread receiver(receiveMessages, clientSocket);

    char message[1000];

    while (true) {

        cout << "You: ";
        cin.getline(message, 1000);

        send(clientSocket,
             message,
             strlen(message) + 1,
             0);

        if (strcmp(message, "exit") == 0)
            break;
    }

    receiver.join();

    close(clientSocket);

    return 0;
}