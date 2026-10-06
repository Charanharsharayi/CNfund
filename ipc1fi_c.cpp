#include <iostream>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <cstdint>

using namespace std;

#define C2S_FIFO "/tmp/client_to_server_fifo"
#define S2C_FIFO "/tmp/server_to_client_fifo"

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

static_assert(sizeof(Packet) == 80);


void read_full(int fd, void *buffer, size_t size) {

    char *p = (char*)buffer;
    size_t received = 0;

    while (received < size) {

        ssize_t n =
            read(fd, p + received, size - received);

        if (n <= 0) {
            perror("read");
            exit(1);
        }

        received += n;
    }
}


void write_full(int fd,
                const void *buffer,
                size_t size) {

    const char *p = (const char*)buffer;
    size_t sent = 0;

    while (sent < size) {

        ssize_t n =
            write(fd, p + sent, size - sent);

        if (n <= 0) {
            perror("write");
            exit(1);
        }

        sent += n;
    }
}


string timestamp() {

    time_t now = time(nullptr);

    tm *t = localtime(&now);

    char buf[30];

    strftime(buf,
             sizeof(buf),
             "%Y-%m-%d %H:%M:%S",
             t);

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

    if (p.payload_len > 0)
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


void sendPacket(int fd, Packet p) {

    write_full(fd, &p, sizeof(Packet));

    logPacket("CLIENT -> SERVER", p);
}


Packet receivePacket(int fd) {

    Packet p{};

    read_full(fd, &p, sizeof(Packet));

    logPacket("SERVER -> CLIENT", p);

    return p;
}


int main() {

    // Start server first.

    int c2s = open(C2S_FIFO, O_RDWR);
    int s2c = open(S2C_FIFO, O_RDWR);

    if (c2s < 0 || s2c < 0) {
        perror("open");
        return 1;
    }


    cout << "FIFO Client started\n";


    for (int connection = 1;
         connection <= 3;
         connection++) {

        cout << "\n====================================\n";
        cout << " CONNECTION " << connection << endl;
        cout << "====================================\n";


        uint32_t clientSeq =
            100 + connection;


        // ---------------- SYN ----------------

        Packet syn = makePacket(
            CLIENT_ID,
            SERVER_ID,
            clientSeq,
            0,
            SYN
        );

        sendPacket(c2s, syn);


        // ------------- SYN + ACK -------------

        Packet synAck = receivePacket(s2c);

        if (!(synAck.flags & SYN) ||
            !(synAck.flags & ACK)) {

            cout << "Invalid SYN+ACK\n";
            return 1;
        }


        // ---------------- ACK ----------------

        Packet ack = makePacket(
            CLIENT_ID,
            SERVER_ID,
            clientSeq + 1,
            synAck.seq_num + 1,
            ACK
        );

        sendPacket(c2s, ack);

        cout << "Handshake completed\n";


        // ---------------- DATA ---------------

        string message =
            "Hello from Client connection " +
            to_string(connection);

        Packet data = makePacket(
            CLIENT_ID,
            SERVER_ID,
            clientSeq + 1,
            synAck.seq_num + 1,
            DATA,
            message
        );

        sendPacket(c2s, data);


        // Receive ACK or NACK

        Packet dataAck = receivePacket(s2c);

        if (dataAck.payload_len == 4 &&
            string(dataAck.payload, 4) == "NACK") {

            cout << "NACK received. Retransmitting...\n";

            sendPacket(c2s, data);
        }


        // ---------------- FIN ----------------

        Packet fin = receivePacket(s2c);

        if (!(fin.flags & FIN)) {
            cout << "Expected FIN\n";
            return 1;
        }


        // ---------------- ACK ----------------

        Packet finAck = makePacket(
            CLIENT_ID,
            SERVER_ID,
            data.seq_num + data.payload_len,
            fin.seq_num + 1,
            ACK
        );

        sendPacket(c2s, finAck);


        cout << "Connection "
             << connection
             << " closed.\n";
    }


    close(c2s);
    close(s2c);

    return 0;
}