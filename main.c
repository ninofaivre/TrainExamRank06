#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

typedef struct sClient {
    int fd;
    int id;
    struct sClient *next;
}   tClient;

tClient *clients = NULL;
int nextId, serverSocketFd = 0;
fd_set sockets, readSockets, writeSockets;

void error(void) {
    fprintf(stderr, "Fatal error\n");
    if (serverSocketFd != -1)
        close(serverSocketFd);
    exit(1);
}

void sendAll(char *buffer, size_t length, int sender) {
    tClient *ptrClients = clients;

    while (ptrClients) {
        if (ptrClients->fd != sender && FD_ISSET(ptrClients->fd, &writeSockets)) {
            if (send(ptrClients->fd, buffer, length, 0) <= 0)
                error();
        }
        ptrClients = ptrClients->next;
    }
}

tClient *getLastClient() {
    if (!clients)
        return NULL;
    tClient *ptrClients = clients;
    while (ptrClients->next)
        ptrClients = ptrClients->next;
    return ptrClients;
}

void addClient() {
    const int newFd = accept(serverSocketFd, NULL, NULL);

    tClient *newClient = malloc(sizeof(tClient));
    if (!newClient)
        error();
    newClient->fd = newFd;
    newClient->id = nextId++;
    newClient->next = NULL;

    tClient *lastClient = getLastClient();
    if (!lastClient)
        clients = newClient;
    else
        lastClient->next = newClient;
    FD_SET(newClient->fd, &sockets);
    char buff[64];
    sprintf(buff, "server: client %d just arrived\n", newClient->id);
    sendAll(buff, strlen(buff), newClient->fd);
}

int getIdOfFd(int fd) {
    tClient *ptrClients = clients;
    
    while (ptrClients) {
        if (ptrClients->fd == fd)
            return ptrClients->id;
        ptrClients = ptrClients->next;
    }
    return -1;
}

void delClient(int fd) {
    tClient *ptrClients = clients;

    if (!ptrClients)
        return ;

    if (ptrClients->fd == fd) {
        free(ptrClients);
        clients = NULL;
        FD_CLR(fd, &sockets);
        close(fd);
        return ;
    }

    while(ptrClients->next && ptrClients->next->fd != fd)
        ptrClients = ptrClients->next;

    if (!ptrClients->next)
        return ;

    tClient *next = ptrClients->next->next;
    int leavingId = ptrClients->next->id;
    free(ptrClients->next);
    ptrClients->next = next;

    FD_CLR(fd, &sockets);
    close(fd);

    char buff[64];
    sprintf(buff, "server: client %d just left\n", leavingId);
    sendAll(buff, strlen(buff), fd);
}

int getMaxFd() {
    tClient *ptrClients = clients;
    int maxFd = serverSocketFd;

    while (ptrClients) {
        if (ptrClients->fd > maxFd)
            maxFd = ptrClients->fd;
        ptrClients = ptrClients->next;
    }

    return maxFd;
}

int main(int argc, char **argv) {

    // check argc
    if (argc != 2) {
        fprintf(stderr, "Wrong number of arguments\n");
        exit(1);
    }

    // sockaddr init
    struct sockaddr_in serverAddr;
    bzero(&serverAddr, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    serverAddr.sin_port = htons(atoi(argv[1]));

    // server init
    if ((serverSocketFd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
        error();
    if (bind(serverSocketFd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) != 0)
        error();
    if (listen(serverSocketFd, 128) == -1)
        error();

    FD_ZERO(&sockets);
    FD_SET(serverSocketFd, &sockets);

    while(1) {
        writeSockets = readSockets = sockets;

        if (select(getMaxFd() + 1, &readSockets, &writeSockets, NULL, NULL) < 0)
            continue ;

        for (int fd = serverSocketFd; fd < getMaxFd() + 1; fd++) {

            if (!FD_ISSET(fd, &readSockets))
                continue ;

            if (fd == serverSocketFd) {
                addClient();
                break ;
            }

            char c;
            int recvCode = recv(fd, &c, 1, 0);

            if (recvCode == 1) {
                char tmp[64];
                sprintf(tmp, "client %d: %c", getIdOfFd(fd), c);
                sendAll(tmp, strlen(tmp), fd);
            }
            while (recvCode == 1 && c != '\n') {
                recvCode = recv(fd, &c, 1, 0);
                if (recvCode == 1)
                    sendAll(&c, 1, fd);
            }
            
            if (recvCode <= 0) { 
                delClient(fd);
                break ;
            }

        }

    }

    return 0;
}
