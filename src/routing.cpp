/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <sys/un.h>

#include "debug.h"
#include "routing.h"

//https://stackoverflow.com/questions/8104904/identify-program-that-connects-to-a-unix-domain-socket
#define MAX_RECEIVED_MESSAGES_SIZE 50000000 //50 MB
namespace echolib {

inline SharedDictionary generate_error_command(long key, const std::string &message) {

    SharedDictionary command = generate_command(ECHO_COMMAND_ERROR);
    command->set<string>("error", message);
    command->set<int>("key",key);

    return command;

}

inline SharedDictionary generate_confirm_command(long key) {

    SharedDictionary command = generate_command(ECHO_COMMAND_OK);
    command->set<int>("key",key);

    return command;

}

inline SharedDictionary generate_event_command(int channel) {

    SharedDictionary command = generate_command(ECHO_COMMAND_EVENT);
    command->set<int>("channel", channel);
    return command;

}

Client::Client(int sfd): fd(sfd), reader(sfd), writer(sfd), connected(true) {
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

Client::~Client() {


}

SharedMessage Client::read() {
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

bool Client::is_connected() {
    return connected;
}

bool Client::disconnect() {
    if(!close(fd)) {
        DEBUGMSG("Disconnecting client FID=%d\n", fd);
    } else {
        perror("close");
    }
    connected = false;
    return true;

}

void Client::send(const SharedMessage message) {
    writer.add_message(message, 0);
}

bool Client::write() {
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

int Client::get_process() const {
    return process_id;
}

int Client::get_user() const {
    return user_id;
}

int Client::get_group() const {

    return group_id;
}

int Client::get_identifier() const {
    return fd;
}

Channel::Channel(int identifier, SharedClient owner, const string& type) : identifier(identifier), type(type), owner(owner) {

}

Channel::~Channel() {

}

string Channel::get_type() const {
    return type;
}

bool Channel::publish(SharedClient client, SharedMessage message) {

    // TODO: CHECK PERMISSION !
    std::vector<SharedClient> to_remove;
    for (std::set<SharedClient>::iterator it = subscribers.begin(); it != subscribers.end(); ++it) {
        if((*it)->is_connected()) {
            (*it)->send(message);
        } else  {
            to_remove.push_back(*it);
        }

    }
    //removing in top loop would invalidate the iterator
    if(to_remove.size()>0) {
        for (auto it = to_remove.begin(); it != to_remove.end(); ++it) {
            unsubscribe(*it);
        }
    }
    return true;

}

bool Channel::subscribe(SharedClient client) {
    if (!is_subscribed(client)) {

        subscribers.insert(client);
        DEBUGMSG("Client FID=%d has subscribed to channel %d (%ld total)\n",
                 client->get_identifier(), get_identifier(), subscribers.size());

        SharedDictionary status = generate_event_command(get_identifier());
        status->set<int>("subscribers", subscribers.size());
        status->set<string>("type", "subscribe");
        SharedMessage message = Message::pack<Dictionary>(ECHO_CONTROL_CHANNEL, *status);

        for (std::set<SharedClient>::iterator it = watchers.begin(); it != watchers.end(); ++it) {
            (*it)->send(message);
        }

        return true;

    }

    return false;
}

bool Channel::unsubscribe(SharedClient client) {

    if (is_subscribed(client)) {

        subscribers.erase(client);
        DEBUGMSG("Client FID=%d has unsubscribed from channel %d (%ld total)\n",
                 client->get_identifier(), get_identifier(), subscribers.size());

        SharedDictionary status = generate_event_command(get_identifier());
        status->set<int>("subscribers", subscribers.size());
        status->set<string>("type", "unsubscribe");
        SharedMessage message = Message::pack<Dictionary>(ECHO_CONTROL_CHANNEL, *status);

        for (std::set<SharedClient>::iterator it = watchers.begin(); it != watchers.end(); ++it) {
            (*it)->send(message);
        }

        return true;

    }

    return false;
}

bool Channel::watch(SharedClient client) {
    if (!is_watching(client)) {

        watchers.insert(client);
        DEBUGMSG("Client FID=%d is watching channel %d\n", client->get_identifier(), get_identifier());

        SharedDictionary status = generate_event_command(get_identifier());
        status->set<int>("subscribers", subscribers.size());
        status->set<string>("type", "summary");
        SharedMessage message = Message::pack<Dictionary>(ECHO_CONTROL_CHANNEL, *status);
        client->send(message);

        return true;

    }

    return false;
}

bool Channel::unwatch(SharedClient client) {

    if (is_watching(client)) {

        watchers.erase(client);
        DEBUGMSG("Client FID=%d has stopped watching channel %d\n", client->get_identifier(), get_identifier());
        return true;

    }

    return false;
}


bool Channel::is_subscribed(SharedClient client) {

    return (subscribers.find(client) != subscribers.end());

}

bool Channel::is_watching(SharedClient client) {

    return (watchers.find(client) != watchers.end());

}

int Channel::get_identifier() const {
    return identifier;
}

Router::Router() : next_channel_id(1), received_messages_size(0) {

}

Router::~Router() {


}

void Router::handle_client(int sfd) {
    if (clients.find(sfd) == clients.end()) {
        DEBUGMSG("Connecting client FID=%d\n", sfd);
        clients[sfd] = make_shared<Client>(sfd);
    } else if(!clients[sfd]->is_connected()) {
        clients.erase(sfd);
        clients[sfd] = make_shared<Client>(sfd);
    }

    SharedClient client = clients[sfd];
    while (true) {
        SharedMessage msg = client->read();

        if (msg) {
            received_messages_size+=(msg->get_length());
            handle_message(client, msg);
            //if we have received too many messages we should send the ones we currently have
            if(received_messages_size > MAX_RECEIVED_MESSAGES_SIZE) {
                handle_write();
            }
        } else {
            if (!client->is_connected()) {
                clients.erase(sfd);

            }
            return;

        }

    }


}

bool Router::handle_write() {
    received_messages_size=0;
    bool done = true;
    for (std::map<int, SharedClient>::iterator it = clients.begin(); it != clients.end(); it++) {
        done &= it->second->write();
    }
    return done;
}

bool Router::disconnect_client(int sfd) {
    if(clients[sfd]) {
        bool result = clients[sfd]->disconnect();

        //unsubscribe and unwatch all channels
        for(auto ch : channels) {
            ch.second->unsubscribe(clients[sfd]);
            ch.second->unwatch(clients[sfd]);
        }
        clients.erase(sfd);
        return result;

    } else {
        DEBUGMSG("Client does not exist FID=%d\n", sfd);
        clients.erase(sfd);
        return false;
    }
}

//TODO: Better error handling so that we cannot crash daemon
void Router::handle_message(SharedClient client, SharedMessage message) {
    if (message->get_channel() == ECHO_CONTROL_CHANNEL) {
        SharedDictionary command = Message::unpack<Dictionary>(message);
        SharedDictionary response = handle_command(client, command);
        if (response) {
            client->send(Message::pack<Dictionary>(ECHO_CONTROL_CHANNEL, *response));
        }
        return;
    }
    // Does the channel exist?
    if (channels.find(message->get_channel()) == channels.end()) {
        DEBUGMSG("Channel with ID=%d does not exist\n", message->get_channel());
        return;
    }

    // Distribute the message
    // TODO: has to change the source first!
    channels[message->get_channel()]->publish(client, message);

}

SharedChannel Router::create_channel(const string &alias, SharedClient creator, const string& type) {

    if (aliases.find(alias) != aliases.end())
        return channels[aliases[alias]];

    int channel_id = next_channel_id++;

    DEBUGMSG("Creating channel %d (alias: %s, type: %s)\n", channel_id, alias.c_str(), type.c_str());

    SharedChannel channel(make_shared<Channel>(channel_id, creator, type));
    channels[channel_id] = channel;
    aliases[alias] = channel_id;

    return channel;

}

SharedDictionary Router::handle_command(SharedClient client, SharedDictionary command) {
    if (!command->contains("key")) {
        DEBUGMSG("Received illegal command message\n");
        return SharedDictionary();
    }

    long key = command->get<int>("key", -1);
    switch (command->get<int>("code", -1)) {
    case ECHO_COMMAND_LOOKUP: {
        string channel_alias = command->get<string>("alias", "");
        string channel_type = command->get<string>("type", "");
        bool create = command->get<bool>("create", true);
        if (channel_alias.size() == 0) {
            return generate_error_command(key, "Channel argument not provided or illegal");
        }

        bool found = aliases.find(channel_alias) != aliases.end();

        if (!found && !create)
            return generate_error_command(key, "Channel does not exist");

        if(create) {
            if(!found) {
                SharedChannel channel = create_channel(channel_alias, client, channel_type);
            }
        }

        int id = aliases[channel_alias];

        if (channel_type.empty() || channels[id]->get_type() == channel_type) {
            SharedDictionary command = generate_command(ECHO_COMMAND_RESULT);
            command->set<string>("alias", channel_alias);
            command->set<string>("type", channels[id]->get_type());
            command->set<int>("channel", id);
            command->set<int>("key", key);
            return command;
        } else {

            return generate_error_command(key, "Channel type does not match");
        }

    }
    case ECHO_COMMAND_SUBSCRIBE: {
        int channel_id = command->get<int>("channel", ECHO_COMMAND_UNKNOWN);

        if (channels.find(channel_id) == channels.end()) {

            return generate_error_command(key, "Channel does not exist");

        }

        if (!channels[channel_id]->subscribe(client)) {

            return generate_error_command(key, "Already subscribed");

        }

        return generate_confirm_command(key);

    }
    case ECHO_COMMAND_SUBSCRIBE_ALIAS: {
        string channel_alias = command->get<string>("channel", "");
        auto channel_id = aliases[channel_alias];
        if (channels.find(channel_id) == channels.end()) {
            return generate_error_command(key, "Channel does not exist");

        }

        if (!channels[channel_id]->subscribe(client)) {

            return generate_error_command(key, "Already subscribed");

        }

        auto ret = generate_confirm_command(key);
        ret->set<string>("alias", channel_alias);
        ret->set<int>("channel_id",channel_id);
        return ret;
    }
    case ECHO_COMMAND_CREATE_CHANNEL_WITH_ALIAS: {
        string channel_alias = command->get<string>("channel", "");
        string channel_type = command->get<string>("type", "");
        create_channel(channel_alias, client, channel_type);
        return generate_confirm_command(key);
    }
    case ECHO_COMMAND_UNSUBSCRIBE: {

        int channel_id = command->get<int>("channel", -1);

        if (channels.find(channel_id) == channels.end()) {

            return generate_error_command(key, "Channel does not exist");

        }

        if (!channels[channel_id]->unsubscribe(client)) {

            return generate_error_command(key, "Not subscribed");

        }

        return generate_confirm_command(key);

    }
    case ECHO_COMMAND_WATCH: {

        int channel_id = command->get<int>("channel", -1);

        if (channels.find(channel_id) == channels.end()) {

            return generate_error_command(key, "Channel does not exist");

        }

        if (!channels[channel_id]->watch(client)) {

            return generate_error_command(key, "Already watching");

        }

        return generate_confirm_command(key);

    }
    case ECHO_COMMAND_UNWATCH: {

        int channel_id = command->get<int>("channel", -1);

        if (channels.find(channel_id) == channels.end()) {

            return generate_error_command(key, "Channel does not exist");

        }

        if (!channels[channel_id]->unwatch(client)) {

            return generate_error_command(key, "Not watching");

        }

        return generate_confirm_command(key);

    }


    }

    return generate_error_command(key,"Message unhandled");

}


}
