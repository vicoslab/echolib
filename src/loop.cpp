
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <chrono>
#include <algorithm>

#include "debug.h"
#include <echolib/loop.h>

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

bool IOBase::observe(SharedIOBaseObserver observer) {

    SYNCHRONIZED(mutex);

    if (std::find(observers.begin(), observers.end(), observer) != observers.end())
        return false;

    observers.push_back(observer);

    return true;

}

bool IOBase::unobserve(SharedIOBaseObserver observer) {

    SYNCHRONIZED(mutex);

    auto position = std::find(observers.begin(), observers.end(), observer);
    if (position == observers.end())
        return false;
    else observers.erase(position);

    return true;
}

void IOBase::notify_output() {

    if (observers.size() > 0) {

            SharedIOBase ref = std::dynamic_pointer_cast<IOBase>(shared_from_this());

            for (auto it : observers) {
                (*it).on_output(ref);
            }
        }
}

void IOLoop::add_handler(SharedIOBase base) {

	int fd = base->get_file_descriptor();

	//if (handlers.find(fd) != handlers.end()) {
	//	throw runtime_error("Listener for file descriptor already registered");
	//}

    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN;
    if (epoll_ctl (efd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw runtime_error("Unable to use epoll");
    }

	handlers[fd] = base;

}

void IOLoop::remove_handler(SharedIOBase base) {

	int fd = base->get_file_descriptor();

    DEBUGMSG("Removing client handler FID=%d\n", fd);

	if (handlers.find(fd) == handlers.end()) {
		DEBUGMSG("Listener for file descriptor not registered\n");
        return;
	}

    if (epoll_ctl (efd, EPOLL_CTL_DEL, fd, NULL) == -1) {
        DEBUGMSG("Removing unknown file descriptor from epoll FID=%d\n", fd);
    }

	handlers.erase(handlers.find(fd));

}

bool IOLoop::wait(int64_t timeout) {

    //SYNCHRONIZED(mutex);

    struct epoll_event *events;
    events = (epoll_event *) calloc (MAXEVENTS, sizeof(epoll_event));

    bool write_done = true;

    auto start = std::chrono::system_clock::now();
    while (handlers.size() > 0) {
        auto current = std::chrono::system_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(current - start);

        int64_t remaining = write_done ? -1 : 1;

        if (timeout > 0) {
            remaining = timeout - (duration.count());
            if (remaining < 1)
                break;
            if (!write_done) remaining = 1;
        }
        int n = epoll_wait (efd, events, MAXEVENTS, remaining);
        for (int i = 0; i < n; i++) {
        	int fd = events[i].data.fd;
        	if (handlers.find(fd) == handlers.end()) continue;

			SharedIOBase base = handlers[fd];

            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                base->disconnect();
                remove_handler(base);
            } else if (events[i].events & EPOLLIN) {
                // TODO: handle timeout
                if (!base->handle_input()) {
                    base->disconnect();
                    remove_handler(base);
                }
            } else if (events[i].events & EPOLLOUT) {
                struct epoll_event event;
                event.data.fd = fd;
                event.events = EPOLLOUT;
                if (epoll_ctl (efd, EPOLL_CTL_DEL, event.data.fd, &event) == -1) {
                    throw runtime_error("Error when removing epoll event");
                }
            }
        }

        write_done = true;
        for (std::map<int, SharedIOBase>::iterator it = handlers.begin(); it != handlers.end(); it++) {
            bool done = it->second->handle_output();
            if (!done) {
                struct epoll_event event;
                event.data.fd = it->second->get_file_descriptor();
                event.events = EPOLLOUT;
                if (epoll_ctl (efd, EPOLL_CTL_ADD, event.data.fd, &event) == -1) {
                    if (errno != EEXIST)
                        throw runtime_error("Error when adding epoll event");
                }
            }
            write_done &= done;
        }
     
    }

    free(events);
    return handlers.size() > 0;

}

SharedIOLoop loop;

SharedIOLoop default_loop() {

    if (!loop) {
        loop = make_shared<IOLoop>();
    }

    return loop;

}

bool wait(int64_t timeout) {

    return default_loop()->wait(timeout);

}

}
