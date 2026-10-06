#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

using namespace std;

struct Student {
    int roll;
    int semester;
    vector<string> subjects;
};

vector<Student> students;


/* ---------- Find Student ---------- */

int findStudent(int roll) {

    for (int i = 0; i < students.size(); i++) {
        if (students[i].roll == roll)
            return i;
    }

    return -1;
}


/* ---------- HTTP Response ---------- */

string response(string status, string body) {

    string res;

    res += "HTTP/1.1 " + status + "\r\n";
    res += "Content-Type: application/json\r\n";
    res += "Content-Length: " + to_string(body.size()) + "\r\n";
    res += "Connection: close\r\n";
    res += "\r\n";
    res += body;

    return res;
}


/* ---------- Get Query Parameter ---------- */

string getQuery(string uri, string key) {

    size_t pos = uri.find(key + "=");

    if (pos == string::npos)
        return "";

    pos += key.length() + 1;

    size_t end = uri.find("&", pos);

    if (end == string::npos)
        end = uri.length();

    return uri.substr(pos, end - pos);
}


/* ---------- Check Student ID ---------- */

bool validStudent(int id) {

    return id == 101 || id == 102 || id == 103;
}


/* ---------- Check Faculty ID ---------- */

bool validFaculty(int id) {

    return id == 9001 || id == 9002;
}


/* ---------- Handle Request ---------- */

void handleRequest(int clientSocket) {

    char buffer[10000];

    int n = recv(clientSocket, buffer, sizeof(buffer) - 1, 0);

    if (n <= 0)
        return;

    buffer[n] = '\0';

    string request(buffer);

    cout << "\n========== REQUEST ==========\n";
    cout << request << endl;


    /* ---------- Parse Request Line ---------- */

    string requestLine;

    size_t lineEnd = request.find("\r\n");

    requestLine = request.substr(0, lineEnd);

    string method, uri, version;

    stringstream ss(requestLine);

    ss >> method >> uri >> version;

    cout << "Method: " << method << endl;
    cout << "URI: " << uri << endl;
    cout << "Version: " << version << endl;


    /* ---------- Parse Content-Length ---------- */

    int contentLength = 0;

    size_t cl = request.find("Content-Length:");

    if (cl != string::npos) {

        size_t start = cl + 15;

        size_t end = request.find("\r\n", start);

        contentLength =
            stoi(request.substr(start, end - start));
    }

    cout << "Content-Length: "
         << contentLength << endl;


    /* ---------- Extract Body ---------- */

    string body;

    size_t bodyStart = request.find("\r\n\r\n");

    if (bodyStart != string::npos) {

        bodyStart += 4;

        body = request.substr(bodyStart);
    }


    /* ---------- Get User ID ---------- */

    int userID = 0;

    size_t idPos = request.find("X-ID:");

    if (idPos != string::npos) {

        size_t start = idPos + 5;

        size_t end = request.find("\r\n", start);

        userID =
            stoi(request.substr(start, end - start));
    }


    cout << "User ID: " << userID << endl;


    /* ================================================= */
    /*                    GET                            */
    /* ================================================= */

    if (method == "GET") {

        string rollString = getQuery(uri, "roll");

        if (rollString == "") {

            string body =
                "{\"error\":\"Roll number required\"}";

            send(clientSocket,
                 response("400 Bad Request", body).c_str(),
                 response("400 Bad Request", body).size(),
                 0);

            return;
        }

        int roll = stoi(rollString);

        int index = findStudent(roll);

        if (index == -1) {

            string body =
                "{\"error\":\"Student not found\"}";

            string res =
                response("404 Not Found", body);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }

        Student s = students[index];

        string body = "{";
        body += "\"roll\":" + to_string(s.roll) + ",";
        body += "\"semester\":" + to_string(s.semester) + ",";
        body += "\"subjects\":[";

        for (int i = 0; i < s.subjects.size(); i++) {

            body += "\"" + s.subjects[i] + "\"";

            if (i != s.subjects.size() - 1)
                body += ",";
        }

        body += "]}";

        string res =
            response("200 OK", body);

        send(clientSocket, res.c_str(), res.size(), 0);
    }


    /* ================================================= */
    /*                    POST                           */
    /* ================================================= */

    else if (method == "POST") {

        if (!validStudent(userID)) {

            string body =
                "{\"error\":\"Invalid student ID\"}";

            string res =
                response("403 Forbidden", body);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        /*
            Expected:

            {"roll":101,"semester":6,
             "subjects":["CS2305","CS2306",
                         "CS1124","24CSA0A16"]}
        */

        size_t p1 = body.find("\"roll\":");

        size_t p2 = body.find("\"semester\":");

        if (p1 == string::npos ||
            p2 == string::npos) {

            string msg =
                "{\"error\":\"Invalid JSON\"}";

            string res =
                response("400 Bad Request", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        int roll =
            stoi(body.substr(p1 + 7));

        int semester =
            stoi(body.substr(p2 + 11));


        if (roll != userID) {

            string msg =
                "{\"error\":\"You can register only your own roll number\"}";

            string res =
                response("403 Forbidden", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        if (findStudent(roll) != -1) {

            string msg =
                "{\"error\":\"Student already exists\"}";

            string res =
                response("409 Conflict", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        Student s;

        s.roll = roll;
        s.semester = semester;


        /*
           Extract subjects between [ ]
        */

        size_t start =
            body.find("[", p2);

        size_t end =
            body.find("]", start);

        string subjectPart =
            body.substr(start + 1,
                        end - start - 1);


        stringstream subjects(subjectPart);

        string subject;

        while (getline(subjects, subject, ',')) {

            subject.erase(
                remove(subject.begin(),
                       subject.end(), '"'),
                subject.end());

            s.subjects.push_back(subject);
        }


        students.push_back(s);


        string msg =
            "{\"message\":\"Registration created successfully\"}";

        string res =
            response("201 Created", msg);

        send(clientSocket, res.c_str(), res.size(), 0);
    }


    /* ================================================= */
    /*                    PUT                            */
    /* ================================================= */

    else if (method == "PUT") {

        if (!validStudent(userID)) {

            string msg =
                "{\"error\":\"Invalid student ID\"}";

            string res =
                response("403 Forbidden", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        string rollString =
            getQuery(uri, "roll");

        if (rollString == "") {

            string msg =
                "{\"error\":\"Roll number required\"}";

            string res =
                response("400 Bad Request", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        int roll = stoi(rollString);


        if (roll != userID) {

            string msg =
                "{\"error\":\"You can update only your own record\"}";

            string res =
                response("403 Forbidden", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        int index =
            findStudent(roll);

        if (index == -1) {

            string msg =
                "{\"error\":\"Student not found\"}";

            string res =
                response("404 Not Found", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        size_t p =
            body.find("\"semester\":");

        if (p != string::npos) {

            students[index].semester =
                stoi(body.substr(p + 11));
        }


        students[index].subjects.clear();


        size_t start =
            body.find("[");

        size_t end =
            body.find("]");


        if (start != string::npos &&
            end != string::npos) {

            string subjectPart =
                body.substr(start + 1,
                            end - start - 1);

            stringstream subjects(subjectPart);

            string subject;

            while (getline(subjects, subject, ',')) {

                subject.erase(
                    remove(subject.begin(),
                           subject.end(), '"'),
                    subject.end());

                students[index].subjects.push_back(subject);
            }
        }


        string msg =
            "{\"message\":\"Registration updated successfully\"}";

        string res =
            response("200 OK", msg);

        send(clientSocket, res.c_str(), res.size(), 0);
    }


    /* ================================================= */
    /*                    DELETE                         */
    /* ================================================= */

    else if (method == "DELETE") {

        // Only faculty can delete

        if (!validFaculty(userID)) {

            string msg =
                "{\"error\":\"Only faculty can delete records\"}";

            string res =
                response("403 Forbidden", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        string rollString =
            getQuery(uri, "roll");

        if (rollString == "") {

            string msg =
                "{\"error\":\"Roll number required\"}";

            string res =
                response("400 Bad Request", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        int roll = stoi(rollString);

        int index =
            findStudent(roll);


        if (index == -1) {

            string msg =
                "{\"error\":\"Student not found\"}";

            string res =
                response("404 Not Found", msg);

            send(clientSocket, res.c_str(), res.size(), 0);

            return;
        }


        students.erase(
            students.begin() + index);


        string msg =
            "{\"message\":\"Registration deleted successfully\"}";

        string res =
            response("200 OK", msg);

        send(clientSocket, res.c_str(), res.size(), 0);
    }


    /* ================================================= */
    /*                 UNKNOWN METHOD                   */
    /* ================================================= */

    else {

        string msg =
            "{\"error\":\"Method not supported\"}";

        string res =
            response("405 Method Not Allowed", msg);

        send(clientSocket, res.c_str(), res.size(), 0);
    }
}


/* ================================================= */
/*                    MAIN                           */
/* ================================================= */

int main() {

    int ID;

    cout << "Enter your ID: ";
    cin >> ID;


    int port =
        15000 + (ID % 900);


    cout << "Server Port: "
         << port << endl;


    int serverSocket =
        socket(AF_INET,
               SOCK_STREAM,
               0);


    sockaddr_in serverAddr{};

    serverAddr.sin_family =
        AF_INET;

    serverAddr.sin_port =
        htons(port);

    serverAddr.sin_addr.s_addr =
        INADDR_ANY;


    bind(serverSocket,
         (sockaddr*)&serverAddr,
         sizeof(serverAddr));


    listen(serverSocket, 10);


    cout << "HTTP Server running...\n";


    while (true) {

        int clientSocket =
            accept(serverSocket,
                   nullptr,
                   nullptr);

        cout << "\nClient connected\n";

        handleRequest(clientSocket);

        close(clientSocket);
    }


    close(serverSocket);

    return 0;
}