#include <iostream>
#include <fstream>
#include <cstring>

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


int main() {

    int clientID;

    cout << "Client ID: ";
    cin >> clientID;


    int qid;


    if (clientID == 1) {

        // Wait for server to create queue.

        qid =
            msgget(
                KEY,
                0666
            );

        if (qid < 0) {

            perror("msgget");
            return 1;
        }


        string filename;

        cout << "File name: ";
        cin >> filename;


        ifstream file(
            filename,
            ios::binary
        );


        Packet p{};

        p.client_id = 1;

        strncpy(
            p.filename,
            filename.c_str(),
            99
        );


        file.read(
            p.data,
            MAX_DATA
        );


        p.data_len =
            file.gcount();


        /*
           mtype = 1 means server.
        */

        sendPacket(
            qid,
            1,
            p
        );


        cout << "File uploaded.\n";
    }


    else {

        qid =
            msgget(
                KEY,
                0666
            );


        if (qid < 0) {

            perror("msgget");
            return 1;
        }


        string filename;

        int priority;


        cout << "File name: ";
        cin >> filename;


        cout << "Priority: ";
        cin >> priority;


        Packet request{};

        request.client_id =
            clientID;

        request.priority =
            priority;


        strncpy(
            request.filename,
            filename.c_str(),
            99
        );


        /*
           mtype = 1 means server.
        */

        sendPacket(
            qid,
            1,
            request
        );


        cout << "Request sent.\n";


        ofstream output(
            "received_" + filename,
            ios::binary
        );


        while (true) {

            Packet p =
                receivePacket(
                    qid,
                    clientID
                );


            if (p.data_len == 0)
                break;


            output.write(
                p.data,
                p.data_len
            );
        }


        output.close();


        cout << "File received.\n";


        Packet ack{};

        ack.client_id =
            clientID;


        /*
           ACK goes to server.
        */

        sendPacket(
            qid,
            1,
            ack
        );


        cout << "ACK sent.\n";
    }


    return 0;
}