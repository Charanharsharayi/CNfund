#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <algorithm>
#include <cstring>
#include <ctime>

#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>

using namespace std;

#define C2S_FIFO "/tmp/file_c2s"
#define S2C_FIFO "/tmp/file_s2c"

#define UPLOAD 1
#define REQUEST 2
#define FILE_DATA 3
#define ACK 4

#define MAX_DATA 4096

struct Packet {
    int type;
    int client_id;
    int priority;
    int sequence;
    int data_len;

    char filename[100];

    char data[MAX_DATA];
};


// FIFO is an ordered byte stream like TCP and has no message
// boundaries. Every packet has fixed size, so read_full()
// repeatedly calls read() until exactly one complete Packet
// has been reconstructed. Packet size is also kept below
// PIPE_BUF so one write() is atomic.


void read_full(int fd, void *buffer, size_t size) {

    char *p = (char*)buffer;

    size_t total = 0;

    while (total < size) {

        ssize_t n =
            read(fd, p + total, size - total);

        if (n <= 0) {
            perror("read");
            exit(1);
        }

        total += n;
    }
}


void write_full(int fd,
                const void *buffer,
                size_t size) {

    const char *p = (const char*)buffer;

    size_t total = 0;

    while (total < size) {

        ssize_t n =
            write(fd, p + total, size - total);

        if (n <= 0) {
            perror("write");
            exit(1);
        }

        total += n;
    }
}


struct Request {

    int client_id;
    int priority;
    int arrival;

    string filename;
};


vector<Request> requests;


void logRequest(Request r,
                int order,
                string status) {

    ofstream log("server_log.txt",
                 ios::app);

    time_t now = time(nullptr);

    log << "Client ID: " << r.client_id
        << " | File: " << r.filename
        << " | Priority: " << r.priority
        << " | Request Time: "
        << ctime(&now)
        << " | Processing Order: "
        << order
        << " | Status: "
        << status
        << "\n";

    log.close();
}


int main() {

    // Clean old FIFOs.

    unlink(C2S_FIFO);
    unlink(S2C_FIFO);


    if (mkfifo(C2S_FIFO, 0666) < 0) {
        perror("mkfifo C2S");
        return 1;
    }

    if (mkfifo(S2C_FIFO, 0666) < 0) {
        perror("mkfifo S2C");
        return 1;
    }


    cout << "FIFO Server started.\n";


    // O_RDWR avoids open() deadlock while clients connect.

    int c2s =
        open(C2S_FIFO, O_RDWR);

    int s2c =
        open(S2C_FIFO, O_RDWR);


    if (c2s < 0 || s2c < 0) {
        perror("open FIFO");
        return 1;
    }


    /*
       STEP 1
       Receive file from Client 1
    */

    Packet packet{};

    read_full(c2s,
              &packet,
              sizeof(Packet));


    if (packet.type != UPLOAD) {

        cerr << "Expected upload.\n";
        return 1;
    }


    string filename =
        packet.filename;


    string serverFile =
        "server_" + filename;


    ofstream file(serverFile,
                  ios::binary);


    if (!file) {

        perror("file");
        return 1;
    }


    // Store uploaded data.

    file.write(packet.data,
                packet.data_len);

    file.close();


    cout << "File uploaded by Client 1: "
         << filename << endl;


    /*
       STEP 2
       Receive requests from Clients 2,3,4...
    */

    int numberOfClients = 3;

    cout << "Waiting for "
         << numberOfClients
         << " file requests...\n";


    for (int i = 0;
         i < numberOfClients;
         i++) {

        Packet p{};

        read_full(c2s,
                  &p,
                  sizeof(Packet));


        if (p.type != REQUEST)
            continue;


        Request r;

        r.client_id = p.client_id;
        r.priority = p.priority;
        r.arrival = i;
        r.filename = p.filename;


        requests.push_back(r);


        cout << "Request received: Client "
             << r.client_id
             << " Priority "
             << r.priority
             << endl;
    }


    /*
       STEP 3
       Priority queue

       Lower priority number = higher priority.
       If priorities are equal, arrival order is used.
    */

    sort(requests.begin(),
         requests.end(),
         [](const Request &a,
            const Request &b) {

        if (a.priority != b.priority)
            return a.priority < b.priority;

        return a.arrival < b.arrival;
    });


    cout << "\nProcessing order:\n";


    for (int i = 0;
         i < requests.size();
         i++) {

        cout << i + 1
             << ". Client "
             << requests[i].client_id
             << " Priority "
             << requests[i].priority
             << endl;
    }


    /*
       STEP 4
       Send file according to priority
    */

    for (int i = 0;
         i < requests.size();
         i++) {

        Request r =
            requests[i];


        // Read stored file.

        ifstream input(serverFile,
                       ios::binary);


        if (!input) {

            cerr << "Cannot open stored file.\n";
            continue;
        }


        input.seekg(0, ios::end);

        long fileSize =
            input.tellg();

        input.seekg(0);


        cout << "\nSending file to Client "
             << r.client_id
             << endl;


        int sequence = 0;


        while (fileSize > 0) {

            Packet data{};

            data.type = FILE_DATA;

            data.client_id =
                r.client_id;

            data.sequence =
                sequence++;


            int chunk =
                min((long)MAX_DATA,
                    fileSize);


            input.read(data.data,
                       chunk);


            data.data_len = chunk;


            strncpy(data.filename,
                    filename.c_str(),
                    99);


            /*
               Server-to-client FIFO is shared by all clients.
               client_id tells each client whether this packet
               belongs to it.
            */

            write_full(
                s2c,
                &data,
                sizeof(Packet)
            );


            fileSize -= chunk;
        }


        input.close();


        /*
           Send an empty FILE_DATA packet to indicate EOF.
        */

        Packet endPacket{};

        endPacket.type = FILE_DATA;

        endPacket.client_id =
            r.client_id;

        endPacket.data_len = 0;


        write_full(
            s2c,
            &endPacket,
            sizeof(Packet)
        );


        /*
           Wait for acknowledgement.
        */

        while (true) {

            Packet ack{};

            read_full(c2s,
                      &ack,
                      sizeof(Packet));


            if (ack.type == ACK &&
                ack.client_id ==
                    r.client_id) {

                cout << "ACK received from Client "
                     << r.client_id
                     << endl;

                logRequest(
                    r,
                    i + 1,
                    "COMPLETED"
                );

                break;
            }
        }
    }


    close(c2s);
    close(s2c);


    unlink(C2S_FIFO);
    unlink(S2C_FIFO);


    cout << "\nFile sharing completed.\n";


    return 0;
}