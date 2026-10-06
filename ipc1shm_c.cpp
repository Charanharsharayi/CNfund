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

    cout << "  src_id      = "
         << p.src_id << endl;

    cout << "  dst_id      = "
         << p.dst_id << endl;

    cout << "  seq_num     = "
         << p.seq_num << endl;

    cout << "  ack_num     = "
         << p.ack_num << endl;

    cout << "  flags       = "
         << (int)p.flags << endl;

    cout << "  payload_len = "
         << p.payload_len << endl;

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


void waitSem(int semid, int number) {

    struct sembuf op{};

    op.sem_num = number;
    op.sem_op = -1;
    op.sem_flg = 0;

    semop(semid, &op, 1);
}


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

    // Wait for free slot.
    waitSem(semid, SEM_FREE);

    shared->packet = p;

    // Tell receiver that packet is ready.
    signalSem(semid, SEM_READY);

    logPacket("CLIENT -> SERVER", p);
}


Packet receivePacket(
    SharedData *shared,
    int semid
) {

    // Block until server puts a packet.
    waitSem(semid, SEM_READY);

    Packet p = shared->packet;

    // Slot becomes free.
    signalSem(semid, SEM_FREE);

    logPacket("SERVER -> CLIENT", p);

    return p;
}


int main() {

    cout << "Shared Memory Client started\n";


    int shmid =
        shmget(
            SHM_KEY,
            sizeof(SharedData),
            0666
        );


    int semid =
        semget(
            SEM_KEY,
            2,
            0666
        );


    if (shmid < 0 || semid < 0) {
        perror("IPC access");
        return 1;
    }


    SharedData *shared =
        (SharedData*)shmat(
            shmid,
            nullptr,
            0
        );


    for (int connection = 1;
         connection <= 3;
         connection++) {

        cout << "\n========== CONNECTION "
             << connection
             << " ==========\n";


        uint32_t clientSeq =
            100 + connection;


        // SYN

        sendPacket(
            shared,
            semid,
            makePacket(
                CLIENT_ID,
                SERVER_ID,
                clientSeq,
                0,
                SYN
            )
        );


        // SYN + ACK

        Packet synAck =
            receivePacket(
                shared,
                semid
            );


        if (!(synAck.flags & SYN) ||
            !(synAck.flags & ACK))
            return 1;


        // ACK

        sendPacket(
            shared,
            semid,
            makePacket(
                CLIENT_ID,
                SERVER_ID,
                clientSeq + 1,
                synAck.seq_num + 1,
                ACK
            )
        );


        cout << "Handshake completed\n";


        // DATA

        string text =
            "Hello from SHM Client " +
            to_string(connection);


        Packet data =
            makePacket(
                CLIENT_ID,
                SERVER_ID,
                clientSeq + 1,
                synAck.seq_num + 1,
                DATA,
                text
            );


        sendPacket(
            shared,
            semid,
            data
        );


        // DATA ACK

        Packet dataAck =
            receivePacket(
                shared,
                semid
            );


        if (dataAck.payload_len == 4 &&
            string(dataAck.payload, 4) == "NACK") {

            cout << "NACK received. Retransmitting...\n";

            sendPacket(
                shared,
                semid,
                data
            );

            receivePacket(
                shared,
                semid
            );
        }


        // FIN

        Packet fin =
            receivePacket(
                shared,
                semid
            );


        if (!(fin.flags & FIN))
            return 1;


        // FIN ACK

        sendPacket(
            shared,
            semid,
            makePacket(
                CLIENT_ID,
                SERVER_ID,
                data.seq_num +
                    data.payload_len,
                fin.seq_num + 1,
                ACK
            )
        );


        cout << "Connection closed.\n";
    }


    shmdt(shared);

    return 0;
}