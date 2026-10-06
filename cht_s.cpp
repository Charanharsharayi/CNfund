#include <iostream>
#include <cstring>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

int main() {
    int serverSocket, clientSocket;
    char buffer[1000];
    int choice;

    serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8080);
    serverAddr.sin_addr.s_addr = INADDR_ANY;

    bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr));

    listen(serverSocket, 1);

    cout << "Waiting for client...\n";

    clientSocket = accept(serverSocket, nullptr, nullptr);

    while (true) {

        // Give choices to client
        cout << "\n1. Send\n";
        cout << "2. Receive\n";
        cout << "3. Terminate\n";
        cout << "Enter choice: ";
        cin >> choice;

        send(clientSocket, &choice, sizeof(choice), 0);

        if (choice == 3)
            break;

        cin.ignore();

        if (choice == 1) {
            // Server sends messages
            while (true) {
                cout << "Server: ";
                cin.getline(buffer, 1000);

                send(clientSocket, buffer, strlen(buffer) + 1, 0);

                if (strcmp(buffer, "exit") == 0)
                    break;
            }
        }

        else if (choice == 2) {
            // Server receives messages
            while (true) {
                int n = recv(clientSocket, buffer, 999, 0);

                if (n <= 0)
                    break;

                buffer[n] = '\0';

                cout << "Client: " << buffer << endl;

                if (strcmp(buffer, "exit") == 0)
                    break;
            }
        }
    }

    close(clientSocket);
    close(serverSocket);

    return 0;
}