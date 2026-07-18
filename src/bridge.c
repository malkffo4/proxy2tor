#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <signal.h>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#pragma comment(lib, "Ws2_32.lib")
#else
#include <unistd.h>
#include <fcntl.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#endif

#include "bridge.h"
#include "database.h"
#include "thread_utils.h"
#include "args_parser.h"

#define THREADCOUNT 10

#ifdef _WIN32
typedef SOCKET sock_t;
typedef HANDLE thread_t;
#else
typedef int sock_t;
typedef pthread_t thread_t;
#endif

typedef struct {
    sock_t c2_socket;
    uint16_t c2_port;
    char *c2_domain;
    const char *socks_tor_addr;
    uint16_t socks_tor_port;
} BO;

typedef struct {
    thread_t aThread;
    sock_t Socket;
    int Closed;
} MonTh;

static sock_t listen_socket = -1;
static MonTh Threads[THREADCOUNT];

#ifdef _WIN32
CRITICAL_SECTION ghMutex;
#else
static pthread_mutex_t ghMutex;
#endif

Node *head = NULL;
char* tor_addr;
int tor_port;
volatile int g_stop = 0;

#ifdef _WIN32
static BOOL WINAPI console_handler(DWORD signal) {
        if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
            printf("[Signal] Sig stop!\n");
            g_stop = 1;
            return TRUE;
        }
        return FALSE;
    }
#else
static void signal_handler(int sig) {
        if (sig == SIGINT) {
            printf("[Signal] Sig stop!\n");
            g_stop = 1;
        }
    }
#endif

int random_int(void) {
#ifdef _WIN32
    return rand();
#else
    return random();
#endif
}

void sleep_time(int time) {
#ifdef _WIN32
    Sleep(time * 1000);
#else
    sleep(time);
#endif
}
// Cross-platform wrappers
void socket_stop(sock_t *sock) {
    if (*sock != -1) {
#ifdef _WIN32
        shutdown(*sock, SD_BOTH);
        closesocket(*sock);
#else
        shutdown(*sock, SHUT_RDWR);
        close(*sock);
#endif
        *sock = -1;
    }

    sleep_time(5);
}

int set_nonblocking(sock_t sock) {
#ifdef _WIN32
    u_long mode = 1;
    return ioctlsocket(sock, FIONBIO, &mode);
#else
    return fcntl(sock, F_SETFL, O_NONBLOCK | fcntl(sock, F_GETFL, 0));
#endif
}

int init_socket_system() {
#ifdef _WIN32
    WSADATA wsaData;
    return WSAStartup(MAKEWORD(2, 2), &wsaData);
#else
    return 0;
#endif
}

void cleanup_socket_system() {
#ifdef _WIN32
    WSACleanup();
#endif
}

sock_t socks5_connect(char *c2_domain, int c2_port, char *socks_addr, int socks_port) {
    int res;
    sock_t c2_socket = -1;
    struct sockaddr_in addr_c2;

    char Req1[3] = { 0x05, 0x01, 0x00 }; // SOCKS 5, One Authentication Method, No Authentication
    char TmpReq[4] = { 0x05, 0x01, 0x00, 0x03 }; // SOCKS5, CONNECT, RESERVED, DOMAIN
    char Resp1[2];
    char Resp2[10];
    memset(&Resp2, 0, 10);

    c2_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (c2_socket == -1) {
        perror("[*] Error creating socket");

        return -1;
    }

    memset(&addr_c2, 0, sizeof(struct sockaddr_in));

    addr_c2.sin_family = AF_INET;
    addr_c2.sin_addr.s_addr = inet_addr(socks_addr); 
    addr_c2.sin_port = htons(socks_port);

    printf("[*] Connect to socks5 proxy %s:%d\n", socks_addr, socks_port);

    res = connect(c2_socket, (struct sockaddr*)&addr_c2, sizeof(struct sockaddr_in));
    if (res) {
        perror("[*] Error connecting to SOCKS proxy");

        socket_stop(&c2_socket);
        return -1;
    }

    res = send(c2_socket, Req1, 3, 0);
    if (res <= 0) {
        perror("[*] Error sending request 1");

        socket_stop(&c2_socket);
        return -1;
    }

    res = recv(c2_socket, Resp1, 2, 0);
    if (res <= 0) {
        perror("[*] Error receiving response 1");

        socket_stop(&c2_socket);
        return -1;
    }

    if(Resp1[1] != 0x00) {
        perror("[*] Error Authenticating");

        socket_stop(&c2_socket);
        return -1;
    }

    uint8_t DomainLen = (uint8_t)strlen(c2_domain);
    uint16_t Port = htons(c2_port);

    char *Req2 = calloc(sizeof(char), 4 + 1 + DomainLen + 2);
    if (!Req2) {
        perror("Error malloc Req2");

        socket_stop(&c2_socket);
        return -1;
    }

    printf("[socks5_connect] Trying to connect %s:%d\n", c2_domain, c2_port);

    memcpy(Req2, TmpReq, 4);
    memcpy(Req2 + 4, &DomainLen, 1);
    memcpy(Req2 + 5, c2_domain, DomainLen);
    memcpy(Req2 + 5 + DomainLen, &Port, 2);

    res = send(c2_socket, Req2, 4 + 1 + DomainLen + 2, 0);
    if (res <= 0) {
        perror("[*] Error sending request 2");

        free(Req2);
        socket_stop(&c2_socket);
        return -1;
    }
    free(Req2);

    res = recv(c2_socket, Resp2, 10, 0);
    if (res <= 0) {
        perror("[*] Error receiving response 2");

        socket_stop(&c2_socket);
        return -1;
    }

    if (Resp2[1] != 0x00) {
        perror("[*] Error, Response 2[1] != 0x00");

        socket_stop(&c2_socket);
        return -1;
    }

    if (set_nonblocking(c2_socket) == -1) {
        perror("[*] Error setting non-blocking mode");

        socket_stop(&c2_socket);
        return -1;
    }
    
    printf("[socks5_connect] Connected to %s:%d\n", c2_domain, c2_port);
    fflush(stdout);
    
    return c2_socket;
}

sock_t c2_conn(void) {
    int index_rand = random_int() % count_list(head);
    Node *req_node = get_list_by_index(head, index_rand);

    sock_t socket = socks5_connect(req_node->hostname, req_node->port, tor_addr, tor_port);
    if (socket < 0) {
        printf("[!] Error connect to c2 %s:%d\n", req_node->hostname, req_node->port);
        return -1;
    }

    return socket;
}

static int bind_sock_bridge(const char *bridge_addr, uint16_t bridge_port) {
    struct sockaddr_in addr_loc;

    if ((listen_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("[!] Error creating socket");

        return -1;
    }
    #ifdef _WIN32
    const char opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (int));
    #else
    int opt = 1;
    setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (int));
    #endif
    set_nonblocking(listen_socket);

    memset(&addr_loc, 0, sizeof(struct sockaddr_in));
    addr_loc.sin_family = AF_INET;
    addr_loc.sin_port = htons(bridge_port);
    addr_loc.sin_addr.s_addr = inet_addr(bridge_addr);

    if (bind(listen_socket, (struct sockaddr*)&addr_loc, sizeof(struct sockaddr_in)) == -1) {
        perror("[!] Error binding socket");

        socket_stop(&listen_socket);
        return -1;
    }

    if (listen(listen_socket, THREADCOUNT) == -1) {
        perror("[!] Error listening on socket");

        socket_stop(&listen_socket);
        return -1;
    }

    printf("[*] Binded bridge on %s:%d\n", bridge_addr, bridge_port);

    return 0;
}

rthread_t handle_clients(void_t arg) {
    sock_t c2_socket = -1;
    int socket = *(int*)arg;

    int error_times_sel = 0;
    
    c2_socket = c2_conn();
    if (c2_socket < 0) {
        goto _exit;
    }

    int index_my_thread = 0;
    // mutex_lock(&ghMutex);
    if (mutex_try_lock(&ghMutex) != WAIT_OBJECT_0) {
        goto _exit;
    }
    
    for (int i = 0; i < THREADCOUNT; i++) {
        if (Threads[i].Socket == socket) {
            index_my_thread = i;
        }
    }
    mutex_unlock(&ghMutex);

    while (!Threads[index_my_thread].Closed) {
        fd_set readSet;
        int nfds = 0;
        struct timeval timeo;

        FD_ZERO(&readSet);

        FD_SET(c2_socket, &readSet);
        FD_SET(socket, &readSet);
        
        int max_fd = c2_socket;
        if (socket > max_fd)
            max_fd = socket;

        // mutex_lock(&ghMutex);
        if (mutex_try_lock(&ghMutex) != WAIT_OBJECT_0)
            continue;
        for (int i = 0; i < THREADCOUNT; i++) {
            if (Threads[i].Socket > max_fd) {
                max_fd = Threads[i].Socket;
            }
        }
        mutex_unlock(&ghMutex);

        timeo.tv_usec = 0;
        timeo.tv_sec = 10;
        nfds = select(max_fd + 1, &readSet, NULL, NULL, &timeo);
        if (nfds == -1) {
            perror("[*] Error in select()");

            error_times_sel += 1;
            if (error_times_sel > 10) {
                goto _exit;
            }

            sleep_time(5);
            continue;
        } else if (nfds == 0) {
            sleep_time(5);
            continue;
        } else {
            error_times_sel = 0;
        }

       if (FD_ISSET(socket, &readSet)) {
            char buffer[1024];
            size_t bytesRead, bydesWrite;
            errno = 0;
            bytesRead = recv(socket, buffer, sizeof(buffer), 0);
            if (bytesRead == 0) // Just test, worked when -1
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
                    continue;
                // else
                //     bytesRead = -1; // Just test, worked when 0
            }
            if (bytesRead == -1) { // Just test, worked when 0
                printf("[handle_clients] Lost connection with client (errno = %d) 1\n", errno);

                goto _exit;
            }
            errno = 0;
            bydesWrite = send(c2_socket, buffer, bytesRead, 0);
            if (bydesWrite == 0) // Just test, worked when -1
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
                    continue;
                // else
                    // bydesWrite = -1; // Just test, worked when 0
            }
            if (bydesWrite == -1) { // Just test, worked when 0
                printf("[handle_clients] Lost connection with c2 (errno = %d) 1\n", errno);

                goto _exit;
            }
        }

        if (FD_ISSET(c2_socket, &readSet)) {
            char buffer[1024];
            size_t bytesRead, bydesWrite;
            errno = 0;
            bytesRead = recv(c2_socket, buffer, sizeof(buffer), 0);
            if (bytesRead == 0)
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
                    continue;
                else
                    bytesRead = -1;
            }
            if (bytesRead == -1) {
                printf("[handle_clients] Lost connection with c2_server (errno = %d) 2\n", errno);

                goto _exit;
            }
            errno = 0;
            bydesWrite = send(socket, buffer, bytesRead, 0);
            if (bydesWrite == 0)
            {
                if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
                    continue;
                else
                    bydesWrite = -1;
            }
            if (bydesWrite == -1) {
                printf("[handle_clients] Lost connection with client (errno = %d) 2\n", errno);

                goto _exit;
            }
        }
        fflush(stdout);
    }
    _exit:

    // mutex_lock(&ghMutex);

    Threads[index_my_thread].Closed = 1;
    socket_stop(&c2_socket);
    socket_stop(&Threads[index_my_thread].Socket);

    // mutex_unlock(&ghMutex);
    
    printf("[handle_clients] exit.\n");
    fflush(stdout);

    thread_exit();
}

int init_bridge(ProgramArgs args) {
    int error_times = 0;
    int error_times_drop = 0;
    int ThreadCollectior = 0;

#ifdef _WIN32
    SetConsoleCtrlHandler(console_handler, TRUE);
#else
    signal(SIGINT, signal_handler);
#endif

    tor_addr = args.tor_addr;
    tor_port = args.tor_port;

    if (load_from_file(args.c2_file, &head)) {
        printf("[init_bridge] error load file %s\n", args.c2_file);

        return 1;
    }

    for (int i = 0; i < THREADCOUNT; i++) {
        Threads[i].Socket = -1;
        Threads[i].aThread = 0;
        Threads[i].Closed = 1;
    }
    
    print_list(head);
    
    init_socket_system();

    mutex_init(&ghMutex);

    if(bind_sock_bridge(args.bridge_addr, args.bridge_port)) {
        goto _exit;
    }

    printf("[init_bridge] listened.\n");

    while (!g_stop) {
        int mfd = listen_socket, nfds = 0;
        struct timeval timeo;
        fd_set readSet;

        FD_ZERO(&readSet);
        FD_SET(listen_socket, &readSet);

        mutex_lock(&ghMutex);
        for (int i = 0; i < THREADCOUNT; i++) {
            if (Threads[i].Socket > mfd) {
                mfd = Threads[i].Socket;
            }
        }
        mutex_unlock(&ghMutex);

        timeo.tv_usec = 0;
        timeo.tv_sec = 10;
        nfds = select(mfd + 1, &readSet, NULL, NULL, &timeo);
        if (nfds == -1) {
            perror("[init_bridge] Error in select()");

            if (error_times_drop > 5) {
                error_times_drop = 0;
                error_times = 0;
                socket_stop(&listen_socket);
                sleep_time(5);
                continue;
            }
            if (error_times > 5) {
                error_times = 0;
                error_times_drop += 1;
                socket_stop(&listen_socket);
                sleep_time(1);
            }
            continue;
        }

        if (FD_ISSET(listen_socket, &readSet)) {
            struct sockaddr_in clientAddr;
            socklen_t clientAddrLen = sizeof(clientAddr);
            sock_t client_socket = accept(listen_socket, (struct sockaddr*)&clientAddr, &clientAddrLen);
            if (client_socket == -1) {
                perror("[init_bridge] Error accepting connection");
                continue;
            }

            // setsockopt(client_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof (int));
            set_nonblocking(client_socket);

            printf("[init_bridge] thread_create succees\n");
            mutex_lock(&ghMutex);
            for (int i = 0; i < THREADCOUNT; i++) {
                if (Threads[i].aThread == 0) {
                    Threads[i].Socket = client_socket;
                    if (thread_create(&Threads[i].aThread, handle_clients, (void *)&Threads[i].Socket) != 0) {
                        perror("[init_bridge] thread_create error\n");
                        socket_stop(&client_socket);
                        Threads[i].Socket = -1;
                        break;
                    }
                    Threads[i].Closed = 0;
                    
                    ThreadCollectior += 1;
                    break;
                }
            }
            mutex_unlock(&ghMutex);
        }

        // sleep_time(5);
        
        if (ThreadCollectior > (THREADCOUNT/2)) {
            ThreadCollectior = 0;
            mutex_lock(&ghMutex);
            for (int i = 0; i < THREADCOUNT; i++) {
                if (Threads[i].Closed && Threads[i].aThread != 0) {
                    thread_join(Threads[i].aThread);
                    Threads[i].aThread = 0;
                }
            }
            mutex_unlock(&ghMutex);
        }
        fflush(stdout);
    }

    printf("[init_bridge] Init Exit\n");

    mutex_lock(&ghMutex);
    for (int i = 0; i < THREADCOUNT; i++) {
        if (Threads[i].aThread != 0) {
            socket_stop(&Threads[i].Socket);
            thread_join(Threads[i].aThread);
        }
    }
    mutex_unlock(&ghMutex);

    _exit:
    printf("[init_bridge] Exit Done!\n");

    socket_stop(&listen_socket);
    free_list(head);
    cleanup_socket_system();

    fflush(stdout);

    return 0;
}
