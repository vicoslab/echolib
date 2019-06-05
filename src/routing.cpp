/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <unistd.h>
#include <sys/socket.h>
#include <iostream>
#include <sys/un.h>

#include "debug.h"
#include <echolib/routing.h>

//https://stackoverflow.com/questions/8104904/identify-program-that-connects-to-a-unix-domain-socket
#define MAX_RECEIVED_MESSAGES_SIZE 50000000 //50 MB
namespace echolib {

inline SharedDictionary generate_error_command(int64_t key, const std::string &message) {

    SharedDictionary command = generate_command(ECHO_COMMAND_ERROR);
    command->set<string>("error", message);
    command->set<int>("key", key);

    return command;

}

inline SharedDictionary generate_confirm_command(int64_t key) {

    SharedDictionary command = generate_command(ECHO_COMMAND_OK);
    command->set<int>("key", key);

    return command;

}

inline SharedDictionary generate_event_command(int channel) {

    SharedDictionary command = generate_command(ECHO_COMMAND_EVENT);
    command->set<int>("channel", channel);
    return command;

}

Channel::Channel(int identifier, SharedClientConnection owner, const string& type) : identifier(identifier), type(type), owner(owner) {

}

Channel::~Channel() {

}

string Channel::get_type() const {
    return type;
}

bool Channel::set_type(const string& t) {
    if (type.empty()) {
        type = t;
        DEBUGMSG("Updating type for channel %d (type: %s)\n", identifier, type.c_str());
        return true;
    }
    return false;
}

bool Channel::publish(SharedClientConnection client, SharedMessage message) {

    // TODO: CHECK PERMISSION !
    std::vector<SharedClientConnection> to_remove;
    for (std::set<SharedClientConnection>::iterator it = subscribers.begin(); it != subscribers.end(); ++it) {
        if ((*it)->is_connected()) {
            (*it)->send(message);
        } else  {
            to_remove.push_back(*it);
        }

    }
    //removing in top loop would invalidate the iterator
    if (to_remove.size() > 0) {
        for (auto it = to_remove.begin(); it != to_remove.end(); ++it) {
            unsubscribe(*it);
        }
    }
    return true;

}

bool Channel::subscribe(SharedClientConnection client) {
    if (!is_subscribed(client)) {

        subscribers.insert(client);
        DEBUGMSG("Client FID=%d has subscribed to channel %d (%ld total)\n",
                 client->get_file_descriptor(), get_identifier(), (int64_t) subscribers.size());

        SharedDictionary status = generate_event_command(get_identifier());
        status->set<int>("subscribers", subscribers.size());
        status->set<string>("type", "subscribe");
        SharedMessage message = Message::pack<Dictionary>(*status);
        MessageHandler::set_channel(message, ECHO_CONTROL_CHANNEL);
        for (std::set<SharedClientConnection>::iterator it = watchers.begin(); it != watchers.end(); ++it) {
            (*it)->send(message);
        }

        return true;

    }

    return false;
}

bool Channel::unsubscribe(SharedClientConnection client) {

    if (is_subscribed(client)) {

        subscribers.erase(client);
        DEBUGMSG("Client FID=%d has unsubscribed from channel %d (%ld total)\n",
                 client->get_file_descriptor(), get_identifier(), (int64_t) subscribers.size());

        SharedDictionary status = generate_event_command(get_identifier());
        status->set<int>("subscribers", subscribers.size());
        status->set<string>("type", "unsubscribe");
        SharedMessage message = Message::pack<Dictionary>(*status);
        MessageHandler::set_channel(message, ECHO_CONTROL_CHANNEL);
        for (std::set<SharedClientConnection>::iterator it = watchers.begin(); it != watchers.end(); ++it) {
            (*it)->send(message);
        }

        return true;

    }

    return false;
}

bool Channel::watch(SharedClientConnection client) {
    if (!is_watching(client)) {

        watchers.insert(client);
        DEBUGMSG("Client FID=%d is watching channel %d\n", client->get_file_descriptor(), get_identifier());

        SharedDictionary status = generate_event_command(get_identifier());
        status->set<int>("subscribers", subscribers.size());
        status->set<string>("type", "summary");
        SharedMessage message = Message::pack<Dictionary>(*status);
        MessageHandler::set_channel(message, ECHO_CONTROL_CHANNEL);
        client->send(message);

        return true;

    }

    return false;
}

bool Channel::unwatch(SharedClientConnection client) {

    if (is_watching(client)) {

        watchers.erase(client);
        DEBUGMSG("Client FID=%d has stopped watching channel %d\n", client->get_file_descriptor(), get_identifier());
        return true;

    }

    return false;
}


bool Channel::is_subscribed(SharedClientConnection client) {

    return (subscribers.find(client) != subscribers.end());

}

bool Channel::is_watching(SharedClientConnection client) {

    return (watchers.find(client) != watchers.end());

}

int Channel::get_identifier() const {
    return identifier;
}

Router::Router(SharedIOLoop loop, const std::string& address) : Server(loop, address), next_channel_id(1), clients(&ClientConnection::comparator), received_messages_size(0) {

}

Router::~Router() {


}

string format_bytes(uint64_t b) {

    string suffix = string("b");

    if (b >= 1024) {
        b = b >> 10;
        if (b >= 1024) {
            b = b >> 10;
            if (b >= 1024) {
                b = b >> 10;
                suffix = string("Gb");
            } else suffix = string("Mb");
        } else suffix = string("kb");
    } 

    return to_string(b) + suffix;

}


void Router::print_statistics() const {

    cout << clients.size() << " clients connected" <<  endl;

    cout << "FID\tNAME\t\tOUT\tIN\tDROP" << endl;

    for (auto client : clients) {
        ClientStatistics stats = client->get_statistics();

        string name = client->get_name();

        if (name.empty()) name = "\t";

        cout << client->get_file_descriptor() << "\t" << name << "\t" << format_bytes(stats.data_read) << "\t";

        cout << format_bytes(stats.data_written) << "\t" << format_bytes(stats.data_dropped) << "\t";

        cout << endl;
    }

}

void Router::handle_connect(SharedClientConnection client) {

    clients.insert(client);

}

void Router::handle_disconnect(SharedClientConnection client) {

    //unsubscribe and unwatch all channels
    for (auto ch : channels) {
        ch.second->unsubscribe(client);
        ch.second->unwatch(client);
    }


    clients.erase(client);
}

//TODO: Better error handling so that we cannot crash daemon
void Router::handle_message(SharedClientConnection client, SharedMessage message) {

    received_messages_size += (message->get_length());

    //if we have received too many messages we should send the ones we currently have
    if (received_messages_size > MAX_RECEIVED_MESSAGES_SIZE) {
        //    handle_write();
    }

    if (message->get_channel() == ECHO_CONTROL_CHANNEL) {
        SharedDictionary command = Message::unpack<Dictionary>(message);
        SharedDictionary response = handle_command(client, command);
        if (response) {
            SharedMessage message = Message::pack<Dictionary>(*response);
            MessageHandler::set_channel(message, ECHO_CONTROL_CHANNEL);
            client->send(message);
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

SharedChannel Router::create_channel(const string &alias, SharedClientConnection creator, const string& type) {

    if (aliases.find(alias) != aliases.end())
        return channels[aliases[alias]];

    int channel_id = next_channel_id++;

    DEBUGMSG("Creating channel %d (alias: %s, type: %s)\n", channel_id, alias.c_str(), type.c_str());

    SharedChannel channel(make_shared<Channel>(channel_id, creator, type));
    channels[channel_id] = channel;
    aliases[alias] = channel_id;

    return channel;

}

SharedClientConnection Router::find(int fid) {

    for (set<SharedClientConnection>::iterator it = clients.begin(); it != clients.end(); it++) {

        if ((*it)->get_file_descriptor() < fid) continue;
        if ((*it)->get_file_descriptor() == fid) return *it;

        break;

    }

    return SharedClientConnection();

}

SharedDictionary Router::handle_command(SharedClientConnection client, SharedDictionary command) {
    if (!command->contains("key")) {
        DEBUGMSG("Received illegal command message from client %s (FID=%d)\n", client->get_name().c_str(),
            client->get_file_descriptor());
        return SharedDictionary();
    }

    int key = command->get<int>("key", -1);
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

        if (create) {
            if (!found) {
                SharedChannel channel = create_channel(channel_alias, client, channel_type);
            }
        }

        int id = aliases[channel_alias];

        if (channel_type.empty() || channels[id]->get_type().empty() || channels[id]->get_type() == channel_type) {
            channels[id]->set_type(channel_type);
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
        ret->set<int>("channel_id", channel_id);
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
    case ECHO_COMMAND_SET_NAME: {

        string name = command->get<string>("name", "");

        client->set_name(name);

        return generate_confirm_command(key);

    }
    case ECHO_COMMAND_GET_NAME: {

        int fid = command->get<int>("fid", -1);

        SharedClientConnection r = find(fid);

        if (!r) return generate_error_command(key, "Not found");

        SharedDictionary command = generate_command(ECHO_COMMAND_RESULT);
        command->set<int>("fid", fid);
        command->set<string>("name", r->get_name());
        return command;
    }

    }

    return generate_error_command(key, "Message unhandled");

}


}
