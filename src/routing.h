/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_ROUTING_HPP_
#define ECHO_ROUTING_HPP_

#include "message.h"
#include <map>
#include <vector>
#include <set>

using namespace std;

namespace echolib {

class Client;
typedef std::shared_ptr<Client> SharedClient;

class Channel {

  public:

    Channel(int identifier, SharedClient owner, const string& type = string());
    ~Channel();

    bool publish(SharedClient client, SharedMessage message);

    bool subscribe(SharedClient client);
    bool unsubscribe(SharedClient client);

    bool watch(SharedClient client);
    bool unwatch(SharedClient client);

    bool is_subscribed(SharedClient client);
    bool is_watching(SharedClient client);

    string get_type() const;
    int get_identifier() const;

  private:

    int identifier;
    string type;

    SharedClient owner;
    set<SharedClient> subscribers;
    set<SharedClient> watchers;

};


typedef std::shared_ptr<Channel> SharedChannel;

class Client {

  public:

    Client(int sfd);
    ~Client();

    SharedMessage read();

    bool is_connected();

    bool disconnect();

    void send(const SharedMessage message);

    bool write();

    int get_process() const;

    int get_user() const;

    int get_group() const;

    int get_identifier() const;

  private:

    int fd;

    StreamReader reader;
    StreamWriter writer;

    bool connected;

    int process_id;
    int user_id;
    int group_id;

};


class Router {

  public:
    Router();
    ~Router();

    void add_client(int sfd);
    void handle_client(int sfd);
    bool disconnect_client(int sfd);
    bool handle_write();

  private:

    int next_channel_id;

    void handle_message(SharedClient client, SharedMessage message);

    SharedChannel create_channel(const string& alias, SharedClient owner, const string& type = string());

    SharedDictionary handle_command(SharedClient client, SharedDictionary command);

    map<string, int> aliases;
    map<int, SharedClient> clients;
    map<int, SharedChannel> channels;

    long received_messages_size;

};

}


#endif
