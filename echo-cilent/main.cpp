#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h> // getaddrinfo(), struct addrinfo
#include <arpa/inet.h>
#include <sys/socket.h> // getaddrinfo(), socket(), bind(), connect()
#include <iostream>
#include <thread>

void myerror(const char* msg) { fprintf(stderr, "%s %s %d\n", msg, strerror(errno), errno); }
void usage() {
    printf("syntax : echo-client <ip> <port>\n");
    printf("sample : echo-client 192.168.10.2 1234\n");
}

void recvThread(int sd) {
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
    }
    printf("disconnected\n");
    fflush(stdout);
    ::close(sd);
    exit(0);
}

int main(int argc, char* argv[]) {
    // 인자값 개수만 검사, 주소오류는 getaddrinfo에서
    if (argc != 3){
        usage();
        return -1;
    }

    // getaddrinfo
    struct addrinfo aiInput, *aiOutput, *ai; //aiInput, aiOutput은 getaddrinfo 사용하기 위해 선언
    memset(&aiInput, 0, sizeof(aiInput));
    aiInput.ai_family = AF_INET;
    aiInput.ai_socktype = SOCK_STREAM;
    aiInput.ai_protocol = IPPROTO_TCP;

    int res = getaddrinfo(argv[1], argv[2], &aiInput, &aiOutput); // getaddrinfo()는 호스트명으로 인자를 넣으면 해당하는 IP주소들을 반환함
    if (res != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(res));
        return -1;
    }

    // socket
    int sd; //SocketDescriptor
    for (ai = aiOutput; ai != nullptr; ai = ai->ai_next) { // getaddrinfo()로 얻은 여러개의 주소정보로 소켓을 만듦
        sd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (sd != -1) break;
    }
    if (ai == nullptr) {
        fprintf(stderr, "cann not find socket for %s\n", argv[1]);
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
    // connect
    {
        int res = ::connect(sd, ai->ai_addr, ai->ai_addrlen);
        if (res == -1) {
            myerror("connect");
            return -1;
        }
    }
    std::thread t(recvThread, sd);
    t.detach();

    while (true) {
        std::string s;
        std::getline(std::cin, s);
        s += "\r\n";
        ssize_t res = ::send(sd, s.data(), s.size(), 0);
        if (res == 0 || res == -1) {
            fprintf(stderr, "send return %ld", res);
            myerror(" ");
            break;
        }
    }
    ::close(sd);
}
