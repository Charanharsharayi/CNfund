#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

int main() {
    int clientSocket;
    char buffer[1000];
    int choice;

    clientSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);

    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    connect(clientSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    while (true) {

        // Receive choice from server
        recv(clientSocket, &choice, sizeof(choice), 0);

        if (choice == 3) {
            cout << "Connection terminated.\n";
            break;
        }

        if (choice == 1) {
            // Client sends messages
            cin.ignore();

            while (true) {
                cout << "Client: ";
                cin.getline(buffer, 1000);

                send(clientSocket, buffer, strlen(buffer) + 1, 0);

                if (strcmp(buffer, "exit") == 0)
                    break;
            }
        }

        else if (choice == 2) {
            // Client receives messages
            while (true) {
                int n = recv(clientSocket, buffer, 999, 0);

                if (n <= 0)
                    break;

                buffer[n] = '\0';

                cout << "Server: " << buffer << endl;

                if (strcmp(buffer, "exit") == 0)
                    break;
            }
        }
    }

    close(clientSocket);

    return 0;
}