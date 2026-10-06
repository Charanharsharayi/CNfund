#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cstring>
#include <ctime>

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


struct Request {

    int client_id;
    int priority;
    int arrival;

    string filename;
};


vector<Request> requests;


union semun {

    int val;
    unsigned short *array;
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

        perror("semop wait");
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

        perror("semop signal");
        exit(1);
    }
}


void sendPacket(
    SharedMemory *shared,
    int semid,
    Packet p
) {

    // Wait until shared-memory slot is free.

    waitSem(
        semid,
        0
    );


    shared->packet =
        p;


    // Tell receiver that packet is ready.

    signalSem(
        semid,
        1
    );
}


Packet receivePacket(
    SharedMemory *shared,
    int semid
) {

    // Wait for packet.

    waitSem(
        semid,
        1
    );


    Packet p =
        shared->packet;


    // Slot becomes available again.

    signalSem(
        semid,
        0
    );


    return p;
}


void logRequest(
    Request r,
    int order,
    string status
) {

    ofstream log(
        "server_log.txt",
        ios::app
    );


    time_t now =
        time(nullptr);


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

    /*
       Create shared memory.
    */

    int shmid =
        shmget(
            SHM_KEY,
            sizeof(SharedMemory),
            IPC_CREAT | 0666
        );


    if (shmid < 0) {

        perror("shmget");
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
       Two semaphores:

       semaphore 0 = FREE
       semaphore 1 = READY

       FREE = 1
       READY = 0
    */

    int semid =
        semget(
            SEM_KEY,
            2,
            IPC_CREAT | 0666
        );


    if (semid < 0) {

        perror("semget");
        return 1;
    }


    union semun arg;


    arg.val = 1;

    semctl(
        semid,
        0,
        SETVAL,
        arg
    );


    arg.val = 0;

    semctl(
        semid,
        1,
        SETVAL,
        arg
    );


    cout << "Shared memory server started.\n";


    /*
       Receive Client 1 upload.
    */

    Packet upload =
        receivePacket(
            shared,
            semid
        );


    if (upload.client_id != 1) {

        cerr << "Expected Client 1.\n";
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
       Receive requests.
    */

    for (int i = 0;
         i < 3;
         i++) {

        Packet p =
            receivePacket(
                shared,
                semid
            );


        Request r;

        r.client_id =
            p.client_id;

        r.priority =
            p.priority;

        r.arrival =
            i;

        r.filename =
            p.filename;


        requests.push_back(r);


        cout << "Request: Client "
             << r.client_id
             << " Priority "
             << r.priority
             << endl;
    }


    /*
       Priority + FCFS.
    */

    sort(
        requests.begin(),
        requests.end(),
        [](Request &a,
           Request &b) {

            if (a.priority !=
                b.priority)

                return
                    a.priority <
                    b.priority;


            return
                a.arrival <
                b.arrival;
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

            p.type = 3;

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


            sendPacket(
                shared,
                semid,
                p
            );


            fileSize -= chunk;
        }


        input.close();


        /*
           EOF packet.
        */

        Packet eof{};

        eof.type = 3;

        eof.client_id =
            r.client_id;

        eof.data_len =
            0;


        sendPacket(
            shared,
            semid,
            eof
        );


        /*
           Wait for ACK.
        */

        Packet ack =
            receivePacket(
                shared,
                semid
            );


        if (ack.client_id ==
            r.client_id) {

            cout << "ACK received from Client "
                 << r.client_id
                 << endl;


            logRequest(
                r,
                i + 1,
                "COMPLETED"
            );
        }
    }


    /*
       Cleanup.
    */

    shmdt(shared);

    shmctl(
        shmid,
        IPC_RMID,
        nullptr
    );


    semctl(
        semid,
        0,
        IPC_RMID
    );


    cout << "File sharing completed.\n";


    return 0;
}