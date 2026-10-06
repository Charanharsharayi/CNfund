#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <ctime>

#include <sys/ipc.h>
#include <sys/msg.h>

using namespace std;

#define KEY 0x1234

#define UPLOAD 1
#define REQUEST 2
#define FILE_DATA 3
#define ACK 4

#define MAX_DATA 4096


struct Packet {

    int client_id;
    int priority;
    int sequence;
    int data_len;

    char filename[100];

    char data[MAX_DATA];
};


struct Message {

    long mtype;

    Packet packet;
};


struct Request {

    int client_id;
    int priority;
    int arrival;

    string filename;
};


vector<Request> requests;


void sendPacket(int qid,
                long destination,
                Packet p) {

    Message msg{};

    msg.mtype = destination;
    msg.packet = p;


    if (msgsnd(
            qid,
            &msg.packet,
            sizeof(Packet),
            0) < 0) {

        perror("msgsnd");
        exit(1);
    }
}


Packet receivePacket(int qid,
                     long type) {

    Message msg{};

    if (msgrcv(
            qid,
            &msg.packet,
            sizeof(Packet),
            type,
            0) < 0) {

        perror("msgrcv");
        exit(1);
    }


    return msg.packet;
}


void logRequest(Request r,
                int order,
                string status) {

    ofstream log(
        "server_log.txt",
        ios::app
    );


    time_t now = time(nullptr);


    log << "Client ID: "
        << r.client_id

        << " | File: "
        << r.filename

        << " | Priority: "
        << r.priority

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

    int qid =
        msgget(
            KEY,
            IPC_CREAT | 0666
        );


    if (qid < 0) {

        perror("msgget");
        return 1;
    }


    cout << "Message Queue Server started.\n";


    /*
       Client 1 uploads.
       Client 1 -> server has destination mtype = 0.
       We use type 1 for server's incoming packets.
    */

    Packet upload =
        receivePacket(qid, 1);


    if (upload.client_id != 1) {

        cerr << "Expected Client 1 upload.\n";
        return 1;
    }


    string filename =
        upload.filename;


    string serverFile =
        "server_" + filename;


    ofstream file(
        serverFile,
        ios::binary
    );


    file.write(
        upload.data,
        upload.data_len
    );


    file.close();


    cout << "File stored: "
         << serverFile
         << endl;


    /*
       Receive 3 requests.
    */

    for (int i = 0;
         i < 3;
         i++) {

        Packet p =
            receivePacket(qid, 1);


        Request r;

        r.client_id =
            p.client_id;

        r.priority =
            p.priority;

        r.arrival = i;

        r.filename =
            p.filename;


        requests.push_back(r);


        cout << "Request from Client "
             << r.client_id
             << " Priority "
             << r.priority
             << endl;
    }


    /*
       Priority queue ordering:
       priority 1 first.
       Equal priorities -> FCFS.
    */

    sort(
        requests.begin(),
        requests.end(),
        [](Request &a,
           Request &b) {

            if (a.priority != b.priority)
                return a.priority < b.priority;

            return a.arrival < b.arrival;
        }
    );


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
       Send file.
    */

    for (int i = 0;
         i < requests.size();
         i++) {

        Request r =
            requests[i];


        ifstream input(
            serverFile,
            ios::binary
        );


        input.seekg(
            0,
            ios::end
        );


        long fileSize =
            input.tellg();


        input.seekg(0);


        int sequence = 0;


        while (fileSize > 0) {

            Packet p{};

            p.client_id =
                r.client_id;

            p.sequence =
                sequence++;


            int chunk =
                min(
                    (long)MAX_DATA,
                    fileSize
                );


            input.read(
                p.data,
                chunk
            );


            p.data_len =
                chunk;


            strncpy(
                p.filename,
                filename.c_str(),
                99
            );


            sendPacket(
                qid,
                r.client_id,
                p
            );


            fileSize -= chunk;
        }


        input.close();


        // EOF packet

        Packet eof{};

        eof.client_id =
            r.client_id;

        eof.data_len = 0;


        sendPacket(
            qid,
            r.client_id,
            eof
        );


        /*
           ACK comes back to server using mtype = 1.
        */

        Packet ack =
            receivePacket(
                qid,
                1
            );


        if (ack.client_id ==
            r.client_id) {

            cout << "ACK from Client "
                 << r.client_id
                 << endl;


            logRequest(
                r,
                i + 1,
                "COMPLETED"
            );
        }
    }


    msgctl(
        qid,
        IPC_RMID,
        nullptr
    );


    cout << "File sharing completed.\n";


    return 0;
}