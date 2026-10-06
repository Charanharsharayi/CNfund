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

    logPacket("CLIENT -> SERVER", p);
}


Packet receivePacket(int qid) {

    Message msg{};

    msgrcv(qid,
           &msg.packet,
           sizeof(Packet),
           SERVER_ID,
           0);

    logPacket("SERVER -> CLIENT",
              msg.packet);

    return msg.packet;
}


int main() {

    int qid =
        msgget(QUEUE_KEY, 0666);

    if (qid < 0) {
        perror("msgget");
        return 1;
    }


    cout << "Message Queue Client started\n";


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
            qid,
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
            receivePacket(qid);


        if (!(synAck.flags & SYN) ||
            !(synAck.flags & ACK))
            return 1;


        // ACK

        sendPacket(
            qid,
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
            "Hello from MQ Client " +
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


        sendPacket(qid, data);


        // DATA ACK

        Packet ack =
            receivePacket(qid);


        if (ack.payload_len == 4 &&
            string(ack.payload, 4) == "NACK") {

            cout << "NACK received.\n";

            sendPacket(qid, data);

            receivePacket(qid);
        }


        // FIN

        Packet fin =
            receivePacket(qid);


        // FIN ACK

        sendPacket(
            qid,
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


    return 0;
}