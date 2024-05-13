#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <thread>

#define MAX_CLIENTS 5 // 최대 클라이언트 수

void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }

void usage() {
    printf("syntax : echo-server <port> [-e[-b]]\n");
    printf("sample : echo-server 1234 -e -b\n");
}

struct Param {
    bool echo{false};
    bool broadcast{false};
    uint16_t port{0};

    bool parse(int argc, char* argv[]) {
        for (int i = 1; i < argc;) {
            if (strcmp(argv[i], "-e") == 0) {
                echo = true;
                i++;
                continue;
            }
            if (strcmp(argv[i], "-b") == 0) {
                broadcast = true;
                i++;
                continue;
            }
            if (i < argc) port = atoi(argv[i++]);
        }
        return port != 0;
    }
} param;

int csds[MAX_CLIENTS] = {0}; //Client Socket DescriptorS
int cc = 0; //ClientCount

void recvThread(int sd, int* csds, int* cc) {
    printf("connected\n");
    fflush(stdout);
    static const int BUFSIZE = 65536;
    char buf[BUFSIZE];
    while (true) {
        ssize_t res = ::recv(sd, buf, BUFSIZE - 1, 0);
        if (res == 0 || res == -1) {
            fprintf(stderr, "recv return %ld", res);
            myerror(" ");
            break;
        }
        buf[res] = '\0';
        printf("%s", buf);
        fflush(stdout);
        if (param.echo) {
            if (param.broadcast){
                for (int i=0;i<MAX_CLIENTS; ++i){
                    if (csds[i] !=0){
                        ssize_t broadcastRes = ::send(csds[i], buf, res, 0);
                        if (broadcastRes == 0 || broadcastRes == -1) {
                            fprintf(stderr, "send return %ld", broadcastRes);
                            myerror(" ");
                            break;
                        }
                    }
                }
            }
            else {
                res = ::send(sd, buf, res, 0);
                if (res == 0 || res == -1) {
                    fprintf(stderr, "send return %ld", res);
                    myerror(" ");
                    break;
                }
            }
        }
    }
    printf("disconnected\n");
    fflush(stdout);
    ::close(sd);
}


int main(int argc, char* argv[]) {
    if (!param.parse(argc, argv)) {
        usage();
        return -1;
    }
    // socket
    int sd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        myerror("socket");
        return -1;
    }
    // setsockopt
    {
        int optval = 1;
        int res = ::setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
        if (res == -1) {
            myerror("setsockopt");
            return -1;
        }
    }
    // bind
    {
        struct sockaddr_in addr;
        addr.sin_family = AF_INET;
        addr.sin_port = htons(param.port);

        ssize_t res = ::bind(sd, (struct sockaddr *)&addr, sizeof(addr));
        if (res == -1) {
            myerror("bind");
            return -1;
        }
    }
    // listen
    {
        int res = listen(sd, MAX_CLIENTS); // 동시 수용 가능 Client 5수
        if (res == -1) {
            myerror("listen");
            return -1;
        }
    }
    // accept, Create recvThread
    while (true) {
        struct sockaddr_in addr;
        socklen_t len = sizeof(addr);
        int newsd = ::accept(sd, (struct sockaddr *)&addr, &len);
        if (newsd == -1) {
            myerror("accept");
            break;
        }

        if (cc < MAX_CLIENTS){ // 최대 클라이언트 수 판별
            csds[cc++] = newsd;
            std::thread* t = new std::thread(recvThread, newsd, csds, &cc);
            t->detach();
        } else {
            myerror("maximum clients");
            ::close(newsd);
        }
    }
    ::close(sd);
}
