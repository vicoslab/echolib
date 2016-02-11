/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#include "debug.h"
#include "routing.h"

using namespace echolib;

#define MAXEVENTS 640

#define SOCKET_BUFFER_SIZE 1024 * 1024

// https://stackoverflow.com/questions/8104904/identify-program-that-connects-to-a-unix-domain-socket

typedef struct statistics {


} statistics;


static int make_socket_non_blocking(int sfd) {
    int flags, s;

    flags = fcntl(sfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl(sfd, F_SETFL, flags);
    if (s == -1) {
        perror("fcntl");
        return -1;
    }

    return 0;
}


static bool set_buffer_size(int fd, int size) {
    setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
    setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
    return true;
}

static int create_and_bind(char *path) {
    struct sockaddr_un hints;
    int s, len;
    if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        return -1;
    }
    hints.sun_family = AF_UNIX;  // local is declared before socket()
    strcpy(hints.sun_path, path); // path and name for socket
    unlink(hints.sun_path); // remove socket if already in use
    len = strlen(hints.sun_path) + sizeof(hints.sun_family);

    // associate the socket with the file descriptor s
    if (bind(s, (struct sockaddr *) &hints, len) == -1) {
        perror("bind");
        return -2;
    }

    DEBUGMSG("Succesfully created socket FID:%d\n", s);

    return s;

}

static int create_and_bind_IP(char* port) {

    struct sockaddr_in hints;
    int s;
    if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        perror("socket");
        return -1;
    }
    // IPv4 for now
    // TODO: IPv6 support
    hints.sin_family = AF_INET;
    // bind the created socket to all network interfaces
    hints.sin_addr.s_addr = htonl(INADDR_ANY);
    hints.sin_port = htons(atoi(port));
    printf("%s\n",port);
    printf("%d\n",htons(atoi(port)));

    /* Mark as re-usable (accept more than one connection to same socket) */
    int so_optval = 1;
    if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *) &so_optval,
                   sizeof(SO_REUSEADDR)) < 0) {
        perror("re-usable");
        return -3;
    }


    if (bind(s, (struct sockaddr *) &hints, sizeof(hints)) == -1) {
        perror("bind");
        return -2;
    }
    DEBUGMSG("Succesfully created socket FID:%d\n", s);

    return s;

}

int main(int argc, char *argv[]) {

    int sfd, s;
    int efd;
    struct epoll_event event;

    __debug_enable();

    // Valgrind reports error otherwise: http://stackoverflow.com/questions/19364942/points-to-uninitialised-bytes-valgrind-errors
    memset(&event, 0, sizeof(epoll_event));
    struct epoll_event *events;
    int timeout = -1;
    Router router;


    if (argc != 2 && argc!=3) {
        fprintf(stderr, "Usage: %s [socket_path] or %s -i [port]\n", argv[0],argv[0]);
        exit(EXIT_FAILURE);
    }

    DEBUGMSG("Starting socket binding \n");
    char const* inet_switch = "-i";
    if(strcmp(argv[1],inet_switch)!=0) {
        sfd = create_and_bind(argv[1]);
        //if we have -i as first argument, we will create an internet socket
        //Port is the second argument
    } else {
        printf("Making internet socket\n");
        sfd=create_and_bind_IP(argv[2]);
    }
    if (sfd == -1)
        abort();

    DEBUGMSG("Starting making socket nonblocking\n");
    s = make_socket_non_blocking(sfd);
    if (s == -1)
        abort();

    s = listen(sfd, SOMAXCONN);
    if (s == -1) {
        perror("listen");
        abort();
    }

    efd = epoll_create1(0);
    if (efd == -1) {
        perror("epoll_create");
        abort();
    }

    event.data.fd = sfd;
    event.events = EPOLLIN | EPOLLET | EPOLLOUT;
    s = epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
    if (s == -1) {
        perror("epoll_ctl");
        abort();
    }

    /* Buffer where events are returned */
    events = (epoll_event *) calloc(MAXEVENTS, sizeof event);

    /* The event loop */
    while (1) {
        int n, i;
        n = epoll_wait(efd, events, MAXEVENTS, timeout);
        if (n < 0) {
            perror("epoll");
        }
        for (i = 0; i < n; ++i) {


            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) ||
                    ((!(events[i].events & EPOLLIN)) && !(events[i].events & EPOLLOUT))) {

                //client disconnect
                if (events[i].events & EPOLLHUP) {
                    int disconnected_fd = events[i].data.fd;
                    router.disconnect_client(disconnected_fd);
                    close(disconnected_fd);
                    continue;
                }

                int error = 0;
                socklen_t errlen = sizeof(error);
                if (getsockopt(events[i].data.fd, SOL_SOCKET, SO_ERROR, (void *) &error, &errlen) == 0) {
                    perror("getsockopt");
                }
                continue;
            } else if (sfd == events[i].data.fd) {

                /* We have a notification on the listening socket, which
                   means one or more incoming connections. */
                while (1) {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;

                    in_len = sizeof in_addr;
                    infd = accept(sfd, &in_addr, &in_len);
                    if (infd == -1) {
                        if ((errno == EAGAIN) ||
                                (errno == EWOULDBLOCK)) {
                            /* We have processed all incoming
                               connections. */
                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }
                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */
                    s = make_socket_non_blocking(infd);
                    if (s == -1) {
                        perror("abort");
                        abort();
                    }

                    set_buffer_size(infd, SOCKET_BUFFER_SIZE);

                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
                    // Register new socket for events
                    s = epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
                    if (s == -1) {
                        perror("epoll_ctl");
                        abort();
                    }
                }
                continue;
            } else {
                router.handle_client(events[i].data.fd);

            }
        }
        timeout = router.handle_write() ? -1 : 1;
    }

    free(events);

    close(sfd);

    return EXIT_SUCCESS;
}
