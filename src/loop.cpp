
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <chrono>

#include "loop.h"

using namespace std;

#define MAXEVENTS 8

namespace echolib {
	
IOLoop::IOLoop() {

    efd = epoll_create1 (0);
    if (efd == -1) {
        throw runtime_error("Unable to use epoll");
    }

}

IOLoop::~IOLoop() {


}

void IOLoop::add_handler(SharedIOBase base) {

	int fd = base->get_file_descriptor();

	if (handlers.find(fd) != handlers.end()) {
		throw runtime_error("Listener for file descriptor already registered");
	}

    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLOUT;
    if (epoll_ctl (efd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw runtime_error("Unable to use epoll");
    }

	handlers[fd] = base;

}

void IOLoop::remove_handler(SharedIOBase base) {

	int fd = base->get_file_descriptor();

	if (handlers.find(fd) == handlers.end()) {
		throw runtime_error("Listener for file descriptor not registered");
	}

    if (epoll_ctl (efd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        throw runtime_error("Unable to use epoll");
    }

	handlers.erase(handlers.find(fd));

}

bool IOLoop::wait(long timeout) {

    //SYNCHRONIZED(mutex);

    struct epoll_event *events;
    events = (epoll_event *) calloc (MAXEVENTS, sizeof(epoll_event));

    auto start = std::chrono::system_clock::now();
    while (handlers.size() > 0) {
        auto current = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current - start);
        long remaining = timeout - (duration.count());
        if (remaining < 1)
            break;

        int n = epoll_wait (efd, events, MAXEVENTS, remaining);
        for (int i = 0; i < n; i++) {
        	int fd = events[i].data.fd;

        	if (handlers.find(fd) == handlers.end()) continue;

			SharedIOBase base = handlers[fd];

            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                perror("epoll");
                base->disconnect();
                remove_handler(base);
            } else {
        		// TODO: handle timeout
            	if (!base->handle()) {
           			base->disconnect();
                	remove_handler(base);
            	}
            }
        }
    }

    free(events);
    return handlers.size() > 0;

}

}
