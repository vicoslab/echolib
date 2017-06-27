/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_SERVER_HPP_
#define ECHO_SERVER_HPP_

#include <string>
#include <map>
#include <set>
#include <functional>
#include <utility>
#include <mutex>
#include <type_traits>

#include "loop.h"
#include "message.h"

namespace echolib {

typedef struct ClientStatistics {
    unsigned long data_read;
    unsigned long data_written;
    unsigned long data_dropped;
} ClientStatistics;

class ClientConnection;
typedef std::shared_ptr<ClientConnection> SharedClientConnection;

class Server;
typedef std::shared_ptr<Server> SharedServer;

class ClientConnection : public IOBase {
friend Server;
  public:

    ClientConnection(int sfd, SharedServer server);
    virtual ~ClientConnection();

    SharedMessage read();

    bool is_connected();

	virtual bool handle_input();

	virtual bool handle_output();

	virtual void disconnect();

	virtual int get_file_descriptor();

    void send(const SharedMessage message);

    bool write();

    int get_process() const;

    int get_user() const;

    int get_group() const;

    ClientStatistics get_statistics() const;

	static bool comparator(const SharedClientConnection &lhs, const SharedClientConnection &rhs);

  private:

    int fd;

    StreamReader reader;
    StreamWriter writer;

    bool connected;

    int process_id;
    int user_id;
    int group_id;

    SharedServer server;

};

class Server : public IOBase {
friend ClientConnection;
public:
	Server(SharedIOLoop loop, const std::string& address = std::string());

	virtual ~Server();

	virtual int get_file_descriptor();

	virtual bool handle_input();

	virtual bool handle_output();

	virtual void disconnect();

protected:

    virtual void handle_message(SharedClientConnection client, SharedMessage message) = 0;

    virtual void handle_disconnect(SharedClientConnection client) = 0;

    virtual void handle_connect(SharedClientConnection client) = 0;

private:

	SharedIOLoop loop;

	int fd;

};

}

#endif