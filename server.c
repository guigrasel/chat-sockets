// server.c - Chat TCP multi-cliente sem login
// Compilar: gcc -O2 -Wall -Wextra -o server server.c
// Uso: ./server <porta>

#define _POSIX_C_SOURCE 200809L
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define MAX_CLIENTS FD_SETSIZE
#define BUF_SIZE 4096

typedef struct {
    int fd;
} Client;

static volatile sig_atomic_t running = 1;

static void on_sigint(int sig) {
    (void)sig;
    running = 0;
}

static void fatal(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

static void broadcast(Client *clients, int except_fd, const char *msg, size_t len) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].fd > 0 && clients[i].fd != except_fd) {
            send(clients[i].fd, msg, len, 0);
        }
    }
}

static void remove_client(Client *c) {
    if (c->fd > 0) {
        close(c->fd);
        c->fd = 0;
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Uso: %s <porta>\n", argv[0]);
        return EXIT_FAILURE;
    }

    signal(SIGINT, on_sigint);
    int port = atoi(argv[1]);

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) fatal("socket");

    int yes = 1;
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons((uint16_t)port);

    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) fatal("bind");
    if (listen(srv, 16) < 0) fatal("listen");

    Client clients[MAX_CLIENTS];
    memset(clients, 0, sizeof(clients));

    printf("Servidor ouvindo em 0.0.0.0:%d\n", port);

    while (running) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(srv, &rfds);
        int maxfd = srv;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (clients[i].fd > 0) {
                FD_SET(clients[i].fd, &rfds);
                if (clients[i].fd > maxfd) maxfd = clients[i].fd;
            }
        }

        int ready = select(maxfd + 1, &rfds, NULL, NULL, NULL);
        if (ready < 0) {
            if (errno == EINTR) continue;
            fatal("select");
        }

        if (FD_ISSET(srv, &rfds)) {
            struct sockaddr_in cli;
            socklen_t cl = sizeof(cli);
            int cfd = accept(srv, (struct sockaddr *)&cli, &cl);
            if (cfd >= 0) {
                int idx = -1;
                for (int i = 0; i < MAX_CLIENTS; i++)
                    if (clients[i].fd == 0) { idx = i; break; }
                if (idx == -1) {
                    const char *full = "Sala cheia\n";
                    send(cfd, full, strlen(full), 0);
                    close(cfd);
                } else {
                    clients[idx].fd = cfd;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            Client *c = &clients[i];
            if (c->fd > 0 && FD_ISSET(c->fd, &rfds)) {
                char buf[BUF_SIZE];
                ssize_t n = recv(c->fd, buf, sizeof(buf), 0);
                if (n <= 0) {
                    remove_client(c);
                } else {
                    broadcast(clients, c->fd, buf, (size_t)n);
                }
            }
        }
    }

    for (int i = 0; i < MAX_CLIENTS; i++) remove_client(&clients[i]);
    close(srv);
    puts("Servidor finalizado.");
    return 0;
}
