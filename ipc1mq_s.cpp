#include <iostream>
#include <cstring>
#include <ctime>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <cstdint>

using namespace std;

#define QUEUE_KEY 0x12345

#define CLIENT_ID 100
#define SERVER_ID 200

#define SYN  1
#define ACK  2
#define DATA 4
#define FIN  8

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

struct Message {
    long mtype;
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


void sendPacket(int qid, Packet p) {

    Message msg{};

    msg.mtype = p.dst_id;
    msg.packet = p;

    msgsnd(qid,
           &msg.packet,
           sizeof(Packet),
           0);

    logPacket("SERVER -> CLIENT", p);
}


Packet receivePacket(int qid) {

    Message msg{};

    msgrcv(qid,
           &msg.packet,
           sizeof(Packet),
           CLIENT_ID,
           0);

    logPacket("CLIENT -> SERVER",
              msg.packet);

    return msg.packet;
}


int main() {

    int qid =
        msgget(QUEUE_KEY,
               IPC_CREAT | 0666);

    if (qid < 0) {
        perror("msgget");
        return 1;
    }

    cout << "Message Queue Server started\n";


    for (int connection = 1;
         connection <= 3;
         connection++) {

        cout << "\n========== CONNECTION "
             << connection
             << " ==========\n";


        // SYN

        Packet syn = receivePacket(qid);

        uint32_t clientSeq =
            syn.seq_num;

        uint32_t serverSeq =
            1000 + connection;


        // SYN + ACK

        sendPacket(
            qid,
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
            receivePacket(qid);

        if (!(ack.flags & ACK))
            return 1;


        cout << "Handshake completed\n";


        // DATA

        Packet data =
            receivePacket(qid);


        size_t actualLength =
            strnlen(data.payload, 64);


        if (data.payload_len > 64 ||
            actualLength != data.payload_len) {

            sendPacket(
                qid,
                makePacket(
                    SERVER_ID,
                    CLIENT_ID,
                    serverSeq + 1,
                    data.seq_num + 1,
                    ACK,
                    "NACK"
                )
            );

            data = receivePacket(qid);
        }


        cout << "Data received: "
             << string(data.payload,
                       data.payload_len)
             << endl;


        // DATA ACK

        sendPacket(
            qid,
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
            qid,
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
            receivePacket(qid);

        if (!(finAck.flags & ACK))
            return 1;

        cout << "Connection closed.\n";
    }


    // Remove queue after all 3 connections.
    msgctl(qid, IPC_RMID, nullptr);

    cout << "All 3 connections completed.\n";

    return 0;
}