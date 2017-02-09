#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <sys/un.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>

#include "debug.h"
#include "server.h"

#define SOCKET_BUFFER_SIZE 1024 * 1024
#define MAX_SEND_MESSAGE_QUEUE 10000

using namespace std;

namespace echolib {

ClientConnection::ClientConnection(int sfd, SharedServer server): fd(sfd), reader(sfd), writer(sfd, MAX_SEND_MESSAGE_QUEUE), connected(true),  server(server) {
	struct ucred cr;
	socklen_t len;

	if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &cr, &len) < 0) {
		// Unable to determine credentials, use default
		process_id = -1;
		group_id = -1;
		user_id = -1;
	} else {

		process_id = cr.pid;
		group_id = cr.gid;
		user_id = cr.uid;

	}

}

ClientConnection::~ClientConnection() {


}

ClientStatistics ClientConnection::get_statistics() const {

	ClientStatistics s;

	s.data_read = reader.get_read_data();
	s.data_written = writer.get_written_data();
	s.data_dropped = writer.get_dropped_data();

	return s;
}


SharedMessage ClientConnection::read() {
	if (!connected)
		return SharedMessage();
	SharedMessage msg = reader.read_message();

	if (!msg) {
		if (reader.get_error()) {
			disconnect();
		}

	}
	return msg;

}

bool ClientConnection::is_connected() {
	return connected;
}

void ClientConnection::disconnect() {

	server->handle_disconnect(shared_from_this());

	if (!close(fd)) {
		DEBUGMSG("Disconnecting client FID=%d\n", fd);
	} else {
		perror("close");
	}
	connected = false;

}

void ClientConnection::send(const SharedMessage message) {
	writer.add_message(message, 0);
}

bool ClientConnection::write() {
	if (!connected) {
		return false;
	}
	bool status = writer.write_messages();
	if (!status) {
		if (writer.get_error()) {
			DEBUGMSG("Writer error FID=%d\n", fd);
			disconnect();
		}

	}
	return status;
}

int ClientConnection::get_process() const {
	return process_id;
}

int ClientConnection::get_user() const {
	return user_id;
}

int ClientConnection::get_group() const {

	return group_id;
}

int ClientConnection::get_file_descriptor() {
	return fd;
}

bool ClientConnection::handle_input() {

	while (true) {
		SharedMessage msg = read();

		if (msg) {
			server->handle_message(shared_from_this(), msg);
		} else {
			if (!is_connected())
				return false;
			break;
		}

	}

	return true;
}

bool ClientConnection::handle_output() {

	return write();

}

bool ClientConnection::comparator(const SharedClientConnection &lhs, const SharedClientConnection &rhs) {

    return lhs->get_file_descriptor() < rhs->get_file_descriptor();

}

int make_socket_non_blocking(int sfd) {
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


bool configure_socket(int fd, int size) {
	setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
	setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
	//int set = 1;
	//setsockopt(fd, SOL_SOCKET, SO_NOSIGPIPE, (void *)&set, sizeof(int));
	return true;
}

int create_and_bind(const char *path) {
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

int create_and_bind_interface(int port) {

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
	hints.sin_port = htons(port);

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

Server::Server(SharedIOLoop loop, const std::string& address) : loop(loop) {

	int s;
	// Valgrind reports error otherwise: http://stackoverflow.com/questions/19364942/points-to-uninitialised-bytes-valgrind-errors

	std::string taddress = address;

	if (taddress.empty()) {
		if (getenv("ECHOLIB_SOCKET") != NULL) {
			taddress = string(getenv("ECHOLIB_SOCKET"));
		} else {
			taddress = "/tmp/echo.sock";
		}
	}

	size_t split = taddress.find(":");

	if (split != string::npos) {
		string host = taddress.substr(0, split);
		int port = atoi(taddress.substr(split + 1).c_str());
		DEBUGMSG("Binding TCP socket\n");
		fd = create_and_bind_interface(port);
	} else {
		DEBUGMSG("Binding Unix socket\n");
		fd = create_and_bind(taddress.c_str());
	}

	if (fd == -1)
		abort();

	DEBUGMSG("Starting making socket nonblocking\n");
	s = make_socket_non_blocking(fd);
	if (s == -1)
		abort();

	s = listen(fd, SOMAXCONN);
	if (s == -1) {
		perror("listen");
		abort();
	}

}

Server::~Server() {

	disconnect();

}

int Server::get_file_descriptor() {
	return fd;
}

bool Server::handle_input() {

	int s;
	/* We have a notification on the listening socket, which
	   means one or more incoming connections. */
	while (1) {
		struct sockaddr in_addr;
		socklen_t in_len;
		int infd;

		in_len = sizeof in_addr;
		infd = accept(fd, &in_addr, &in_len);
		if (infd == -1) {
			if ((errno == EAGAIN) ||
			        (errno == EWOULDBLOCK)) {
				/* We have processed all incoming
				   connections. */
				break;
			} else {
				perror("accept");
				return false;
			}
		}
		/* Make the incoming socket non-blocking and add it to the
		   list of fds to monitor. */
		s = make_socket_non_blocking(infd);
		if (s == -1) {
			perror("abort");
			abort();
		}

		configure_socket(infd, SOCKET_BUFFER_SIZE);

		DEBUGMSG("Connecting client FID=%d\n", infd);
		SharedClientConnection client = make_shared<ClientConnection>(infd, shared_from_this());
		loop->add_handler(client);
		handle_connect(client);

	}

	return true;
}


bool Server::handle_output() {
	return true;
}

void Server::disconnect() {

	if (fd > 0) {
		DEBUGMSG("Disconnecting.\n");
		close(fd);
		fd = -1;
	}

}


}