#include <iostream>
#include <string>
#include <sstream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

string sendRequest(
    string host,
    int port,
    string request
) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in serverAddr{};

    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);

    inet_pton(AF_INET, host.c_str(), &serverAddr.sin_addr);

    connect(
        sock,
        (sockaddr*)&serverAddr,
        sizeof(serverAddr)
    );

    // Send HTTP request
    send(sock, request.c_str(), request.size(), 0);

    // Receive HTTP response
    char buffer[10000];

    int n = recv(
        sock,
        buffer,
        sizeof(buffer) - 1,
        0
    );

    if (n <= 0) {
        close(sock);
        return "No response";
    }

    buffer[n] = '\0';

    string response(buffer);

    close(sock);

    return response;
}


int main() {

    int ID;

    cout << "Enter your ID: ";
    cin >> ID;

    // Server port
    int port = 15000 + (ID % 900);

    string serverIP;

    cout << "Enter server IP: ";
    cin >> serverIP;


    while (true) {

        cout << "\n============================\n";
        cout << " Student Registration Client\n";
        cout << "============================\n";

        cout << "1. Create Registration\n";
        cout << "2. Retrieve Registration\n";
        cout << "3. Update Registration\n";
        cout << "4. Delete Registration\n";
        cout << "5. Exit\n";

        cout << "Enter choice: ";

        int choice;
        cin >> choice;


        /* ========================================= */
        /* CREATE */
        /* ========================================= */

        if (choice == 1) {

            int roll, semester;

            cout << "Enter Roll Number: ";
            cin >> roll;

            cout << "Enter Semester: ";
            cin >> semester;


            string s1, s2, s3, s4;

            cout << "Enter Subject 1: ";
            cin >> s1;

            cout << "Enter Subject 2: ";
            cin >> s2;

            cout << "Enter Subject 3: ";
            cin >> s3;

            cout << "Enter Subject 4: ";
            cin >> s4;


            string body =
                "{\"roll\":" + to_string(roll) +
                ",\"semester\":" + to_string(semester) +
                ",\"subjects\":[\"" +
                s1 + "\",\"" +
                s2 + "\",\"" +
                s3 + "\",\"" +
                s4 + "\"]}";


            string request;

            request += "POST /students HTTP/1.1\r\n";
            request += "Host: " + serverIP + "\r\n";
            request += "X-ID: " + to_string(ID) + "\r\n";
            request += "Content-Type: application/json\r\n";
            request += "Content-Length: " +
                       to_string(body.size()) + "\r\n";
            request += "Connection: close\r\n";
            request += "\r\n";
            request += body;


            cout << "\nSending request...\n";

            cout << sendRequest(
                serverIP,
                port,
                request
            );
        }


        /* ========================================= */
        /* RETRIEVE */
        /* ========================================= */

        else if (choice == 2) {

            int roll;

            cout << "Enter Roll Number: ";
            cin >> roll;


            string request;

            request +=
                "GET /students?roll=" +
                to_string(roll) +
                " HTTP/1.1\r\n";

            request +=
                "Host: " + serverIP + "\r\n";

            request +=
                "X-ID: " + to_string(ID) + "\r\n";

            request +=
                "Connection: close\r\n";

            request += "\r\n";


            cout << "\nSending request...\n";

            cout << sendRequest(
                serverIP,
                port,
                request
            );
        }


        /* ========================================= */
        /* UPDATE */
        /* ========================================= */

        else if (choice == 3) {

            int roll, semester;

            cout << "Enter Roll Number: ";
            cin >> roll;

            cout << "Enter New Semester: ";
            cin >> semester;


            string s1, s2, s3, s4;

            cout << "Enter Subject 1: ";
            cin >> s1;

            cout << "Enter Subject 2: ";
            cin >> s2;

            cout << "Enter Subject 3: ";
            cin >> s3;

            cout << "Enter Subject 4: ";
            cin >> s4;


            string body =
                "{\"semester\":" +
                to_string(semester) +
                ",\"subjects\":[\"" +
                s1 + "\",\"" +
                s2 + "\",\"" +
                s3 + "\",\"" +
                s4 + "\"]}";


            string request;

            request +=
                "PUT /students?roll=" +
                to_string(roll) +
                " HTTP/1.1\r\n";

            request +=
                "Host: " + serverIP + "\r\n";

            request +=
                "X-ID: " + to_string(ID) + "\r\n";

            request +=
                "Content-Type: application/json\r\n";

            request +=
                "Content-Length: " +
                to_string(body.size()) +
                "\r\n";

            request +=
                "Connection: close\r\n";

            request += "\r\n";

            request += body;


            cout << "\nSending request...\n";

            cout << sendRequest(
                serverIP,
                port,
                request
            );
        }


        /* ========================================= */
        /* DELETE */
        /* ========================================= */

        else if (choice == 4) {

            int roll;

            cout << "Enter Roll Number to delete: ";
            cin >> roll;


            string request;

            request +=
                "DELETE /students?roll=" +
                to_string(roll) +
                " HTTP/1.1\r\n";

            request +=
                "Host: " + serverIP + "\r\n";

            request +=
                "X-ID: " + to_string(ID) + "\r\n";

            request +=
                "Connection: close\r\n";

            request += "\r\n";


            cout << "\nSending request...\n";

            cout << sendRequest(
                serverIP,
                port,
                request
            );
        }


        /* ========================================= */
        /* EXIT */
        /* ========================================= */

        else if (choice == 5) {

            cout << "Client terminated.\n";

            break;
        }


        else {

            cout << "Invalid choice.\n";
        }
    }

    return 0;
}