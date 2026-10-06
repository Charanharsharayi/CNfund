#include <iostream>
#include <fstream>
#include <cstring>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>

using namespace std;

#define SHM_KEY 0x5555
#define SEM_KEY 0x6666

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


struct SharedMemory {

    Packet packet;
};


void waitSem(int semid,
             int semnum) {

    struct sembuf op{};

    op.sem_num =
        semnum;

    op.sem_op =
        -1;

    op.sem_flg =
        0;


    if (semop(
            semid,
            &op,
            1
        ) < 0) {

        perror("semop");
        exit(1);
    }
}


void signalSem(int semid,
               int semnum) {

    struct sembuf op{};

    op.sem_num =
        semnum;

    op.sem_op =
        +1;

    op.sem_flg =
        0;


    if (semop(
            semid,
            &op,
            1
        ) < 0) {

        perror("semop");
        exit(1);
    }
}


void sendPacket(
    SharedMemory *shared,
    int semid,
    Packet p
) {

    /*
       P(FREE)
       Wait until server/client has finished
       using the shared slot.
    */

    waitSem(
        semid,
        0
    );


    shared->packet =
        p;


    /*
       V(READY)
       Notify receiver.
    */

    signalSem(
        semid,
        1
    );
}


Packet receivePacket(
    SharedMemory *shared,
    int semid
) {

    /*
       P(READY)
       Block until a packet is available.
    */

    waitSem(
        semid,
        1
    );


    Packet p =
        shared->packet;


    /*
       V(FREE)
       Allow another packet to be written.
    */

    signalSem(
        semid,
        0
    );


    return p;
}


int main() {

    int clientID;


    cout << "Enter Client ID: ";
    cin >> clientID;


    int shmid =
        shmget(
            SHM_KEY,
            sizeof(SharedMemory),
            0666
        );


    if (shmid < 0) {

        perror("shmget");
        return 1;
    }


    int semid =
        semget(
            SEM_KEY,
            2,
            0666
        );


    if (semid < 0) {

        perror("semget");
        return 1;
    }


    SharedMemory *shared =
        (SharedMemory*)
        shmat(
            shmid,
            nullptr,
            0
        );


    /*
       Client 1 uploads.
    */

    if (clientID == 1) {

        string filename;


        cout << "File name: ";
        cin >> filename;


        ifstream file(
            filename,
            ios::binary
        );


        if (!file) {

            cerr << "Cannot open file.\n";
            return 1;
        }


        Packet p{};

        p.type = 1;

        p.client_id =
            1;


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


        sendPacket(
            shared,
            semid,
            p
        );


        file.close();


        cout << "File uploaded.\n";
    }


    /*
       Other clients request file.
    */

    else {

        string filename;

        int priority;


        cout << "File name: ";
        cin >> filename;


        cout << "Priority: ";
        cin >> priority;


        Packet request{};

        request.type = 2;

        request.client_id =
            clientID;

        request.priority =
            priority;


        strncpy(
            request.filename,
            filename.c_str(),
            99
        );


        sendPacket(
            shared,
            semid,
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
                    shared,
                    semid
                );


            /*
               Because there is only one shared slot,
               the server sends packets sequentially.
            */

            if (p.client_id !=
                clientID)

                continue;


            if (p.data_len == 0)
                break;


            output.write(
                p.data,
                p.data_len
            );
        }


        output.close();


        cout << "File received.\n";


        /*
           Acknowledge server.
        */

        Packet ack{};

        ack.type = 4;

        ack.client_id =
            clientID;


        sendPacket(
            shared,
            semid,
            ack
        );


        cout << "ACK sent.\n";
    }


    shmdt(shared);


    return 0;
}