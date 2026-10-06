#include <iostream>
#include <cstring>
#include <ctime>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <unistd.h>
#include <cstdint>

using namespace std;

#define SHM_KEY 0x5555
#define SEM_KEY 0x6666

#define CLIENT_ID 100
#define SERVER_ID 200

#define SYN  1
#define ACK  2
#define DATA 4
#define FIN  8

#define SEM_FREE  0
#define SEM_READY 1

#pragma pack(push, 1)
struct Packet {
    uint16_t src_id;
    uint16_t dst_id;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t flags;
    uint16_t payload_len;
    char payload[64];
    uint8_t reserved;
};
#pragma pack(pop)


struct SharedData {
    Packet packet;
};


union semun {
    int val;
    unsigned short *array;
};


string timestamp() {

    time_t now = time(nullptr);
    tm *t = localtime(&now);

    char buf[30];

    strftime(buf, sizeof(buf),
             "%Y-%m-%d %H:%M:%S", t);

    return string(buf);
}


void logPacket(string direction,
               const Packet &p) {

    cout << "\n[" << timestamp() << "] "
         << direction << endl;

    cout << "  src_id      = " << p.src_id << endl;
    cout << "  dst_id      = " << p.dst_id << endl;
    cout << "  seq_num     = " << p.seq_num << endl;
    cout << "  ack_num     = " << p.ack_num << endl;
    cout << "  flags       = " << (int)p.flags << endl;
    cout << "  payload_len = " << p.payload_len << endl;

    if (p.payload_len)
        cout << "  payload     = "
             << string(p.payload,
                       p.payload_len)
             << endl;
}


Packet makePacket(
    uint16_t src,
    uint16_t dst,
    uint32_t seq,
    uint32_t ack,
    uint8_t flags,
    string data = ""
) {

    Packet p{};

    p.src_id = src;
    p.dst_id = dst;

    p.seq_num = seq;
    p.ack_num = ack;

    p.flags = flags;

    p.payload_len = data.size();

    if (p.payload_len > 64)
        p.payload_len = 64;

    memcpy(p.payload,
           data.c_str(),
           p.payload_len);

    return p;
}


// P operation: wait/decrement semaphore
void waitSem(int semid, int number) {

    struct sembuf op{};

    op.sem_num = number;
    op.sem_op = -1;
    op.sem_flg = 0;

    semop(semid, &op, 1);
}


// V operation: signal/increment semaphore
void signalSem(int semid, int number) {

    struct sembuf op{};

    op.sem_num = number;
    op.sem_op = +1;
    op.sem_flg = 0;

    semop(semid, &op, 1);
}


void sendPacket(
    SharedData *shared,
    int semid,
    Packet p
) {

    // P(FREE): wait until slot is free
    waitSem(semid, SEM_FREE);

    shared->packet = p;

    // V(READY): notify receiver
    signalSem(semid, SEM_READY);

    logPacket("SERVER -> CLIENT", p);
}


Packet receivePacket(
    SharedData *shared,
    int semid
) {

    // P(READY): block until packet exists
    waitSem(semid, SEM_READY);

    Packet p = shared->packet;

    // V(FREE): slot can now be reused
    signalSem(semid, SEM_FREE);

    logPacket("CLIENT -> SERVER", p);

    return p;
}


int main() {

    // Shared memory is just storage.
    // It does not notify the other process by itself.
    // SEM_READY performs the notification that the OS/network
    // stack would normally provide when data arrives.


    int shmid =
        shmget(
            SHM_KEY,
            sizeof(SharedData),
            IPC_CREAT | 0666
        );


    int semid =
        semget(
            SEM_KEY,
            2,
            IPC_CREAT | 0666
        );


    if (shmid < 0 || semid < 0) {
        perror("IPC creation");
        return 1;
    }


    SharedData *shared =
        (SharedData*)shmat(shmid,
                           nullptr,
                           0);


    // Initialize:
    // FREE = 1
    // READY = 0

    semun arg;

    arg.val = 1;
    semctl(semid,
           SEM_FREE,
           SETVAL,
           arg);

    arg.val = 0;
    semctl(semid,
           SEM_READY,
           SETVAL,
           arg);


    cout << "Shared Memory Server started\n";


    for (int connection = 1;
         connection <= 3;
         connection++) {

        cout << "\n========== CONNECTION "
             << connection
             << " ==========\n";


        // SYN

        Packet syn =
            receivePacket(
                shared,
                semid
            );


        uint32_t clientSeq =
            syn.seq_num;

        uint32_t serverSeq =
            1000 + connection;


        // SYN + ACK

        sendPacket(
            shared,
            semid,
            makePacket(
                SERVER_ID,
                CLIENT_ID,
                serverSeq,
                clientSeq + 1,
                SYN | ACK
            )
        );


        // ACK

        Packet ack =
            receivePacket(
                shared,
                semid
            );


        if (!(ack.flags & ACK))
            return 1;


        cout << "Handshake completed\n";


        // DATA

        Packet data =
            receivePacket(
                shared,
                semid
            );


        size_t actualLength =
            strnlen(data.payload, 64);


        if (data.payload_len > 64 ||
            actualLength != data.payload_len) {

            sendPacket(
                shared,
                semid,
                makePacket(
                    SERVER_ID,
                    CLIENT_ID,
                    serverSeq + 1,
                    data.seq_num + 1,
                    ACK,
                    "NACK"
                )
            );

            data =
                receivePacket(
                    shared,
                    semid
                );
        }


        cout << "Data received: "
             << string(data.payload,
                       data.payload_len)
             << endl;


        // DATA ACK

        sendPacket(
            shared,
            semid,
            makePacket(
                SERVER_ID,
                CLIENT_ID,
                serverSeq + 1,
                data.seq_num +
                    data.payload_len,
                ACK
            )
        );


        // FIN

        sendPacket(
            shared,
            semid,
            makePacket(
                SERVER_ID,
                CLIENT_ID,
                serverSeq + 1,
                data.seq_num +
                    data.payload_len,
                FIN
            )
        );


        // FIN ACK

        Packet finAck =
            receivePacket(
                shared,
                semid
            );


        if (!(finAck.flags & ACK))
            return 1;


        cout << "Connection closed.\n";
    }


    shmdt(shared);

    // Server owns the IPC resources.
    shmctl(shmid, IPC_RMID, nullptr);
    semctl(semid, 0, IPC_RMID);


    cout << "All 3 connections completed.\n";

    return 0;
}