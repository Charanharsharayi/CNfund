#include <iostream>
#include <fstream>
#include <cstring>

#include <unistd.h>
#include <fcntl.h>

using namespace std;

#define C2S_FIFO "/tmp/file_c2s"
#define S2C_FIFO "/tmp/file_s2c"

#define UPLOAD 1
#define REQUEST 2
#define FILE_DATA 3
#define ACK 4

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


void read_full(int fd,
               void *buffer,
               size_t size) {

    char *p = (char*)buffer;

    size_t total = 0;

    while (total < size) {

        ssize_t n =
            read(fd,
                 p + total,
                 size - total);

        if (n <= 0) {
            perror("read");
            exit(1);
        }

        total += n;
    }
}


void write_full(int fd,
                const void *buffer,
                size_t size) {

    const char *p =
        (const char*)buffer;

    size_t total = 0;

    while (total < size) {

        ssize_t n =
            write(fd,
                  p + total,
                  size - total);

        if (n <= 0) {
            perror("write");
            exit(1);
        }

        total += n;
    }
}


int main() {

    int clientID;

    cout << "Enter Client ID: ";
    cin >> clientID;


    int c2s =
        open(C2S_FIFO, O_RDWR);

    int s2c =
        open(S2C_FIFO, O_RDWR);


    if (c2s < 0 || s2c < 0) {

        perror("open FIFO");
        return 1;
    }


    /*
       CLIENT 1
       Upload file
    */

    if (clientID == 1) {

        string filename;

        cout << "Enter file name: ";
        cin >> filename;


        ifstream file(filename,
                      ios::binary);


        if (!file) {

            cerr << "Cannot open file.\n";
            return 1;
        }


        Packet p{};

        p.type = UPLOAD;
        p.client_id = 1;

        strncpy(p.filename,
                filename.c_str(),
                99);


        file.read(p.data,
                  MAX_DATA);


        p.data_len =
            file.gcount();


        write_full(c2s,
                   &p,
                   sizeof(Packet));


        file.close();


        cout << "File uploaded successfully.\n";
    }


    /*
       CLIENT 2+
       Request file
    */

    else {

        string filename;

        int priority;


        cout << "Enter file name: ";
        cin >> filename;


        cout << "Enter priority: ";
        cin >> priority;


        Packet request{};

        request.type = REQUEST;

        request.client_id =
            clientID;

        request.priority =
            priority;


        strncpy(request.filename,
                filename.c_str(),
                99);


        write_full(
            c2s,
            &request,
            sizeof(Packet)
        );


        cout << "Request sent.\n";


        /*
           Wait for server's file.

           Since all clients share the same FIFO,
           keep reading until a packet addressed
           to this client arrives.
        */

        ofstream output(
            "received_" +
            filename,
            ios::binary
        );


        if (!output) {

            cerr << "Cannot create output file.\n";
            return 1;
        }


        while (true) {

            Packet p{};

            read_full(
                s2c,
                &p,
                sizeof(Packet)
            );


            // Ignore packets intended for another client.

            if (p.client_id != clientID)
                continue;


            if (p.type == FILE_DATA) {

                if (p.data_len == 0)
                    break;


                output.write(
                    p.data,
                    p.data_len
                );
            }
        }


        output.close();


        cout << "File received successfully.\n";


        /*
           Send acknowledgement.
        */

        Packet ack{};

        ack.type = ACK;

        ack.client_id =
            clientID;


        write_full(
            c2s,
            &ack,
            sizeof(Packet)
        );


        cout << "ACK sent to server.\n";
    }


    close(c2s);
    close(s2c);


    return 0;
}