/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_ROUTING_HPP_
#define ECHO_ROUTING_HPP_

#include <echolib/message.h>
#include <echolib/server.h>
#include <map>
#include <vector>
#include <set>

using namespace std;

namespace echolib
{

  class Channel
  {

  public:
    Channel(int identifier, SharedClientConnection owner, const string &type = string());
    ~Channel();

    bool publish(SharedClientConnection client, SharedMessage message);

    bool subscribe(SharedClientConnection client);
    bool unsubscribe(SharedClientConnection client);

    bool watch(SharedClientConnection client);
    bool unwatch(SharedClientConnection client);

    bool is_subscribed(SharedClientConnection client);
    bool is_watching(SharedClientConnection client);

    string get_type() const;
    bool set_type(const string &type);
    int get_identifier() const;

  private:
    int identifier;
    string type;

    SharedClientConnection owner;
    set<SharedClientConnection> subscribers;
    set<SharedClientConnection> watchers;
  };

  typedef std::shared_ptr<Channel> SharedChannel;

  typedef set<SharedClientConnection, function<bool(SharedClientConnection, SharedClientConnection)>> ClientSet;

  class Router : public Server
  {
    friend ClientConnection;

  public:
    Router(SharedIOLoop loop, const std::string &address = std::string());
    ~Router();

    void print_statistics() const;

    static bool comparator(const SharedClientConnection &lhs, const SharedClientConnection &rhs);

  private:
    virtual void handle_message(SharedClientConnection client, SharedMessage message);

    virtual void handle_disconnect(SharedClientConnection client);

    virtual void handle_connect(SharedClientConnection client);

    SharedChannel create_channel(const string &alias, SharedClientConnection owner, const string &type = string());

    SharedDictionary handle_command(SharedClientConnection client, SharedDictionary command);

    SharedClientConnection find(int fid);

    int next_channel_id;

    map<string, int> aliases;

    map<int, SharedChannel> channels;

    ClientSet clients;

    int64_t received_messages_size;
  };

}

#endif
