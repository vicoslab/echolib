/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <stdlib.h>

#include "debug.h"
#include "loop.h"
#include "routing.h"
#include "debug.h"

using namespace echolib;
using namespace std;

// https://stackoverflow.com/questions/8104904/identify-program-that-connects-to-a-unix-domain-socket

int main(int argc, char *argv[]) {

    SharedIOLoop loop = make_shared<IOLoop>();

    string address;
    if (argc > 1) {
        address = string(argv[1]);
    }

    shared_ptr<Router> router = make_shared<Router>(loop, address);
    loop->add_handler(router);

    while (true) {

        loop->wait(5000);

        DEBUGGING {
            cout << " --------------------------- Daemon statistics --------------------------------- " <<  endl;
            router->print_statistics();
        }

    }

/*
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

                while (1) {
                    struct sockaddr in_addr;
                    socklen_t in_len;
                    int infd;

                    in_len = sizeof in_addr;
                    infd = accept(sfd, &in_addr, &in_len);
                    if (infd == -1) {
                        if ((errno == EAGAIN) ||
                                (errno == EWOULDBLOCK)) {

                            break;
                        } else {
                            perror("accept");
                            break;
                        }
                    }

                    s = make_socket_non_blocking(infd);
                    if (s == -1) {
                        perror("abort");
                        abort();
                    }

                    configure_socket(infd, SOCKET_BUFFER_SIZE);

                    event.data.fd = infd;
                    event.events = EPOLLIN | EPOLLET;
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

    close(sfd);*/

    return EXIT_SUCCESS;
}
