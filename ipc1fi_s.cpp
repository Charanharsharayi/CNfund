#include <iostream>
#include <iomanip>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
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

static_assert(sizeof(Packet) == 80, "Packet must be fixed-size");


// FIFO is an ordered byte stream like TCP, not UDP.
// Therefore read_full() is needed because FIFO has no message boundaries.

void read_full(int fd, void *buffer, size_t size) {
    char *p = (char*)buffer;
    size_t received = 0;

    while (received < size) {
        ssize_t n = read(fd, p + received, size - received);

        if (n <= 0) {
            perror("read");
            exit(1);
        }

        received += n;
    }
}


void write_full(int fd, const void *buffer, size_t size) {
    const char *p = (const char*)buffer;
    size_t sent = 0;

    while (sent < size) {
        ssize_t n = write(fd, p + sent, size - sent);

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

    strftime(buf, sizeof(buf),
             "%Y-%m-%d %H:%M:%S", t);

    return string(buf);
}


void logPacket(string direction, const Packet &p) {

    cout << "\n[" << timestamp() << "] "
         << direction << endl;

    cout << "  src_id      = " << p.src_id << endl;
    cout << "  dst_id      = " << p.dst_id << endl;
    cout << "  seq_num     = " << p.seq_num << endl;
    cout << "  ack_num     = " << p.ack_num << endl;
    cout << "  flags       = " << (int)p.flags << endl;
    cout << "  payload_len = " << p.payload_len << endl;

    if (p.payload_len > 0)
        cout << "  payload     = "
             << string(p.payload,
                       p.payload_len) << endl;
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

    logPacket("SERVER -> CLIENT", p);
}


Packet receivePacket(int fd) {

    Packet p{};

    // Exactly one complete Packet is reconstructed.
    read_full(fd, &p, sizeof(Packet));

    logPacket("CLIENT -> SERVER", p);

    return p;
}


int main() {

    unlink(C2S_FIFO);
    unlink(S2C_FIFO);

    mkfifo(C2S_FIFO, 0666);
    mkfifo(S2C_FIFO, 0666);

    // O_RDWR on both ends prevents FIFO open() deadlock.
    int c2s = open(C2S_FIFO, O_RDWR);
    int s2c = open(S2C_FIFO, O_RDWR);

    if (c2s < 0 || s2c < 0) {
        perror("open");
        return 1;
    }

    cout << "FIFO Server started\n";


    for (int connection = 1;
         connection <= 3;
         connection++) {

        cout << "\n====================================\n";
        cout << " CONNECTION " << connection << endl;
        cout << "====================================\n";


        // ---------------- SYN ----------------

        Packet syn = receivePacket(c2s);

        if (!(syn.flags & SYN))
            return 1;

        uint32_t clientSeq = syn.seq_num;

        uint32_t serverSeq = 1000 + connection;

        // ------------- SYN + ACK -------------

        Packet synAck = makePacket(
            SERVER_ID,
            CLIENT_ID,
            serverSeq,
            clientSeq + 1,
            SYN | ACK
        );

        sendPacket(s2c, synAck);


        // ---------------- ACK ----------------

        Packet ack = receivePacket(c2s);

        if (!(ack.flags & ACK) ||
            ack.ack_num != serverSeq + 1) {

            cout << "Handshake failed\n";
            return 1;
        }

        cout << "Handshake completed\n";


        // ---------------- DATA ---------------

        Packet data = receivePacket(c2s);

        if (!(data.flags & DATA))
            return 1;


        size_t actualLength =
            strnlen(data.payload, 64);

        if (data.payload_len > 64 ||
            actualLength != data.payload_len) {

            // ACK with NACK marker
            Packet nack = makePacket(
                SERVER_ID,
                CLIENT_ID,
                serverSeq + 1,
                data.seq_num + 1,
                ACK,
                "NACK"
            );

            sendPacket(s2c, nack);

            // Wait for retransmission
            data = receivePacket(c2s);

            actualLength =
                strnlen(data.payload, 64);

            if (data.payload_len > 64 ||
                actualLength != data.payload_len) {

                cout << "Retransmission failed\n";
                return 1;
            }
        }


        cout << "Data accepted: "
             << string(data.payload,
                       data.payload_len)
             << endl;


        // ACK DATA

        Packet dataAck = makePacket(
            SERVER_ID,
            CLIENT_ID,
            serverSeq + 1,
            data.seq_num + data.payload_len,
            ACK
        );

        sendPacket(s2c, dataAck);


        // ---------------- FIN ----------------

        Packet fin = makePacket(
            SERVER_ID,
            CLIENT_ID,
            serverSeq + 1,
            data.seq_num + data.payload_len,
            FIN
        );

        sendPacket(s2c, fin);


        // ---------------- ACK ----------------

        Packet finAck = receivePacket(c2s);

        if (!(finAck.flags & ACK)) {
            cout << "FIN ACK failed\n";
            return 1;
        }

        cout << "Connection "
             << connection
             << " closed gracefully.\n";
    }


    close(c2s);
    close(s2c);

    unlink(C2S_FIFO);
    unlink(S2C_FIFO);

    cout << "\nAll 3 connections completed.\n";

    return 0;
}