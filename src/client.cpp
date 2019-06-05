/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <errno.h>
#include <stdexcept>
#include <exception>
#include <arpa/inet.h>
#include <cmath>
#include <random>
#include <chrono>
#include <netinet/in.h>

#include "debug.h"
#include <echolib/message.h>
#include <echolib/client.h>
#include <echolib/datatypes.h>

#define MAXEVENTS 8

#define SYNCHRONIZED(M) std::lock_guard<std::recursive_mutex> lock (M)
//#define SYNCHRONIZED(M)

namespace echolib {

static int make_socket_non_blocking (int sfd) {
    int flags, s;

    flags = fcntl (sfd, F_GETFL, 0);
    if (flags == -1) {
        perror ("fcntl");
        return -1;
    }

    flags |= O_NONBLOCK;
    s = fcntl (sfd, F_SETFL, flags);
    if (s == -1) {
        perror ("fcntl");
        return -1;
    }

    return 1;
}

static int connect_socket(const string& address) {

    string temp = address;

    if (temp.empty()) {
        if (getenv("ECHOLIB_SOCKET") != NULL) {
            temp = string(getenv("ECHOLIB_SOCKET"));
        } else {
            temp = "/tmp/echo.sock";
        }
    }

    size_t split = temp.find(":");

    if (split != string::npos) {
        string host = temp.substr(0, split);
        int port = atoi(temp.substr(split + 1).c_str());

        int fd;
        struct sockaddr_in remote;

        if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            throw runtime_error("Unable to create socket");
        }

        remote.sin_family = AF_INET;
        inet_pton(AF_INET, host.c_str(), &(remote.sin_addr));
        remote.sin_port = htons(port);
        if (connect(fd, (struct sockaddr *) &remote, sizeof(remote)) == -1) {
            perror("connect");
            throw runtime_error("Unable to connect to host " + host);
        }

        if (!make_socket_non_blocking(fd)) {
            throw runtime_error("Unable to set socket to non-blocking mode");
        }
        return fd;

    } else {
        int len, fd;
        struct sockaddr_un remote;

        if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
            throw runtime_error("Unable to create socket");
        }

        remote.sun_family = AF_UNIX;
        strcpy(remote.sun_path, temp.c_str());

        len = strlen(remote.sun_path) + sizeof(remote.sun_family);
        if (connect(fd, (struct sockaddr *)&remote, len) == -1) {
            throw runtime_error("Unable to connect to socket " + temp);
        }

        if (!make_socket_non_blocking(fd)) {
            throw runtime_error("Unable to set socket to non-blocking mode");
        }

        return fd;
    }

}

SharedClient connect(const string& address, const string& name, SharedIOLoop loop) {

    SharedClient client = make_shared<Client>(name, address);

    loop->add_handler(client);

    return client;

}

Client::Client(const string& name, const string &address) : fd(connect_socket(address)), writer(fd), reader(fd),
    next_request_key(0), subscriptions(), watches() {

    initialize_common();

    connected = true;

    if (!name.empty()) {

        SharedDictionary command = generate_command(ECHO_COMMAND_SET_NAME);
        command->set<string>("name", name);
    
        send_command(command);
    }


}

Client::~Client() {

    disconnect();

}

void Client::initialize_common() {

    if (getenv("ECHOLIB_MAP") != NULL) {
        string s(getenv("ECHOLIB_MAP"));

        size_t last = 0, next = 0;
        string token;
        while (last != string::npos) {
            next = s.find(";", last);
            if (next == string::npos) {
                token = s.substr(last);
                last = next;
            } else {
                token = s.substr(last, next - last);
                last = next + 1;
            }

            size_t split = token.find("=");

            if (split == string::npos) continue;

            string from = token.substr(0, split);
            string to = token.substr(split + 1);

            DEBUGMSG("Mapping alias %s to %s\n", from.c_str(), to.c_str());

            mappings[from] = to;
        }
    }

}

bool Client::handle_input() {

    while (true) {
        SharedMessage msg = reader.read_message();

        if (!msg) {
            if (reader.get_error()) {
                disconnect();
                return is_connected();
            }
            break;
        } else {
            handle_message(msg);
        }

    }

    return is_connected();

}

bool Client::handle_output() {

    bool status = writer.write_messages();
    if (!status) {
        if (writer.get_error()) {
            DEBUGMSG("Writer error: %d\n", writer.get_error());
            disconnect();
        }

    }
    return status;

}

int Client::get_file_descriptor() {

    return fd;

}

int Client::get_queue_size() {
    return writer.get_queue_size();
}

bool Client::is_connected() {

    return connected;
}

void Client::disconnect() {
    SYNCHRONIZED(mutex);

    if (is_connected()) {
        DEBUGMSG("Disconnecting.\n");
        close(fd);
    }

    connected = false;

}

void Client::handle_message(SharedMessage &message) {

    if (message->get_channel() == ECHO_CONTROL_CHANNEL) {
        SharedDictionary response = Message::unpack<Dictionary>(message);
        if (!response->contains("key")) {
            if (response->get<int>("code", ECHO_COMMAND_UNKNOWN) == ECHO_COMMAND_EVENT) {
                int channel = response->get<int>("channel", 0);
                if (watches.find(channel) == watches.end())
                    return;
                auto callbacks = watches[channel];
                set<WatchCallback>::const_iterator iter;
                for (iter = callbacks.begin(); iter != callbacks.end(); ++iter)
                    (*(*iter))(response);
            }
            return;
        }
        int key = response->get<int>("key", -1);
        if (requests.find(key) == requests.end()) return;
        pair<SharedDictionary, function<void(SharedDictionary, SharedDictionary)> > pending = requests[key];
        if (pending.second)
            pending.second(pending.first, response);
        requests.erase(key);

    } else {
        if (subscriptions.find(message->get_channel()) == subscriptions.end())
            return;
        auto callbacks = subscriptions[message->get_channel()];
        set<DataCallback>::const_iterator iter;

        for (iter = callbacks.begin(); iter != callbacks.end(); ++iter)
            (*(*iter))(message);
    }

}


bool Client::subscribe(int channel, DataCallback callback) {
    SYNCHRONIZED(mutex);

    if (subscriptions.find(channel) == subscriptions.end()) {
        DEBUGMSG("Subscribing to channel %d\n", channel);
        //Generate a subscription command message
        SharedDictionary command = generate_command(ECHO_COMMAND_SUBSCRIBE);
        command->set<int>("channel", channel);
        std::function<bool(SharedDictionary, SharedDictionary)>comm_callback = [](SharedDictionary x, SharedDictionary y) {
            return true;
        };
        //add the subscription command to message queue
        send_command(command, comm_callback);
    }

    return subscriptions[channel].insert(callback).second; // Returns pair, the second value is success

}

bool Client::unsubscribe(int channel, DataCallback callback) {
    SYNCHRONIZED(mutex);

    if (subscriptions.find(channel) == subscriptions.end()) {
        DEBUGMSG("Unknown channel %d\n", channel);
        return false;
    }

    if (subscriptions[channel].find(callback) == subscriptions[channel].end()) {
        DEBUGMSG("Unknown callback for channel %d\n", channel);
        return false;
    }

    if (!subscriptions[channel].erase(callback))
        return false;

    if (subscriptions[channel].size() == 0) {
        DEBUGMSG("No more subscribers for %d\n", channel);
        //no more callbacks, we can unsubscribe
        SharedDictionary command = generate_command(ECHO_COMMAND_UNSUBSCRIBE);
        command->set<int>("channel", channel);
        std::function<bool(SharedDictionary, SharedDictionary)>callback = [](SharedDictionary x, SharedDictionary y) {
            return true;
        };
        //add the unsubscribe command to message queue
        send_command(command, callback);
        subscriptions.erase(channel);
    }

    return true;
}

bool Client::watch(int channel, WatchCallback callback) {
    SYNCHRONIZED(mutex);

    if (watches.find(channel) == watches.end()) {
        //Generate a subscription command message
        SharedDictionary command = generate_command(ECHO_COMMAND_WATCH);
        command->set<int>("channel", channel);
        std::function<bool(SharedDictionary, SharedDictionary)>callback = [](SharedDictionary x, SharedDictionary y) {
            return true;
        };
        //add the watch command to message queue
        send_command(command, callback);
    }

    return watches[channel].insert(callback).second;  // Returns pair, the second value is success
}

bool Client::unwatch(int channel, WatchCallback callback) {
    SYNCHRONIZED(mutex);

    if (watches.find(channel) == watches.end()) {
        DEBUGMSG("Unknown channel %d\n", channel);
        return false;
    }

    if (watches[channel].find(callback) == watches[channel].end()) {
        DEBUGMSG("Unknown callback for channel %d\n", channel);
        return false;
    }

    if (!watches[channel].erase(callback))
        return false;

    if (watches[channel].size() == 0) {
        DEBUGMSG("No more watchers for %d\n", channel);
        // No more callbacks, we can unwatch
        SharedDictionary command = generate_command(ECHO_COMMAND_UNWATCH);
        command->set<int>("channel", channel);
        std::function<bool(SharedDictionary, SharedDictionary)>callback = [](SharedDictionary x, SharedDictionary y) {
            return true;
        };
        //add the unwatch command to message queue
        send_command(command, callback);
        watches.erase(channel);
    }

    return true;
}

void Client::send(SharedMessage message, MessageCallback callback, int priority) {

    SYNCHRONIZED(mutex);

    if (!is_connected()) return;

    writer.add_message(message, priority, callback);

}

void Client::send_command(SharedDictionary command, function<bool(SharedDictionary, SharedDictionary)> callback) {

    SYNCHRONIZED(mutex);

    int key = next_request_key++;
    command->set<int>("key", key);

    pair<SharedDictionary, function<bool(SharedDictionary, SharedDictionary)> > pending(command, callback);
    requests[key] = pending;
    shared_ptr<Message> message = Message::pack<Dictionary>(*command);
    MessageHandler::set_channel(message, ECHO_CONTROL_CHANNEL);
    send(message);

}

static bool internal_lookup_callback(SharedDictionary in, SharedDictionary out, function<void(SharedDictionary)> callback) {
    callback(out);
    return true;
}

void Client::lookup_channel(const string &alias, const string &type, function<void(SharedDictionary)> callback, bool create) {

    using namespace std::placeholders;

    // Perform remapping of channels
    string real_alias = alias;
    if (mappings.find(alias) != mappings.end()) {
        real_alias = mappings[alias];
    }

    SYNCHRONIZED(mutex);

    // Create appropriate command
    SharedDictionary command = generate_command(ECHO_COMMAND_LOOKUP);
    command->set<string>("alias", real_alias);
    command->set<string>("type", type);
    command->set<bool>("create", create);
    this->send_command(command, bind(&internal_lookup_callback, _1, _2, callback));
}

void Subscriber::lookup_callback(SharedDictionary lookup) {
    if (lookup->contains("error")) {
        this->on_error(runtime_error("Unable to find channel"));
    }

    this->id = lookup->get<int>("channel", -1);

    subscribe();

    on_ready();
}

void Subscriber::data_callback(SharedMessage chunk) {

    MessageReader reader(chunk);

    int sequence = reader.read_integer();

    if (sequence < 0) {

        shared_ptr<Message> message = make_shared<OffsetBufferMessage>(chunk, reader.get_position());

        (*callback)(message);

    } else {

        int64_t id = reader.read_long();

        bool valid = true;

        if (pending.find(id) != pending.end()) {

            SharedMessage cropped = make_shared<OffsetBufferMessage>(chunk, reader.get_position());

            valid = pending[id]->set_chunk(sequence, cropped);

        } else {

            if (sequence != 0) return;

            if ((int)pending.size() > pending_capacity && pending_capacity > 0) {
                pending.erase(std::prev(pending.end()));
            }

            int64_t length = reader.read_long();
            int chunk_size = reader.read_integer();

            pending[id] = make_shared<ChunkList>(length, chunk_size);

            SharedMessage cropped = make_shared<OffsetBufferMessage>(chunk, reader.get_position());

            valid = pending[id]->set_chunk(sequence, cropped);

        }

        //MemoryBuffer buffer(chunk->get_length());
        //chunk->copy_data(0, buffer.get_buffer(), buffer.get_length());

        //string tmp((char*)buffer.get_buffer(), buffer.get_length());

        if (!valid) {

            pending.erase(id);
            return;

        }

        if (pending[id]->is_complete()) {

            shared_ptr<ChunkList> chunks = pending[id];

            pending.erase(id);

            shared_ptr<Message> message = make_shared<MultiBufferMessage>(chunks->cbegin(), chunks->cend());

            MessageHandler::set_channel(message, chunk->get_channel());

            (*callback)(message);

        }

    }

}

Subscriber::Subscriber(SharedClient client, const string &alias, const string &type, DataCallback callback, int pending_capacity) : 
    client(client), pending_capacity(pending_capacity) {

    using namespace std::placeholders;

    internal_callback = create_data_callback(bind(&Subscriber::data_callback, this, _1));

    this->callback = (callback) ? callback : create_data_callback(bind(&Subscriber::on_message, this, _1));

    client->lookup_channel(alias, type, bind(&Subscriber::lookup_callback, this, _1));

}

Subscriber::~Subscriber() {

    unsubscribe();

}

bool Subscriber::subscribe() {
    if (this->id < 1) return false;
    return client->subscribe(id, internal_callback);
}

bool Subscriber::unsubscribe() {
    if (this->id < 1) return false;
    return client->unsubscribe(id, internal_callback);
}

void Subscriber::on_ready() {

}

void Subscriber::on_message(SharedMessage message) {

}

void Subscriber::on_error(const std::exception& error) {


}

Subscriber::ChunkList::ChunkList(int length, int chunk_size) : vector<SharedMessage>(), chunk_size(chunk_size) {
    int chunks = (int) ceil((double) length / (double) chunk_size);

    resize(chunks, SharedMessage());

}

Subscriber::ChunkList::~ChunkList() {


}

bool Subscriber::ChunkList::set_chunk(int index, SharedMessage& chunk) {

    if (index > 0 && !at(index - 1))
        return false;

    (*this)[index] = chunk;

    return true;

}

bool Subscriber::ChunkList::is_complete() const {

    return (at(size() - 1)) ? true : false;

}

void Watcher::lookup_callback(SharedDictionary lookup) {
    if (lookup->contains("error")) {
        this->on_error(runtime_error("Unable to find channel"));
    }

    this->id = lookup->get<int>("channel", -1);

    watch();

    on_ready();
}

void Watcher::on_ready() {

}

Watcher::Watcher(SharedClient client, const string &alias) : client(client) {

    using namespace std::placeholders;

    callback = create_watch_callback(bind(&Watcher::on_event, this, _1));

    client->lookup_channel(alias, "", bind(&Watcher::lookup_callback, this, _1));

}

Watcher::~Watcher() {

    unwatch();

}

bool Watcher::watch() {
    if (this->id < 1) return false;
    return client->watch(id, callback);
}

bool Watcher::unwatch() {
    if (this->id < 1) return false;
    return client->unwatch(id, callback);
}

void Publisher::lookup_callback(const string alias, SharedDictionary lookup) {

    if (lookup->contains("error")) {
        DEBUGMSG("Publisher error: %s\n", lookup->get<string>("error").c_str());
        return;
    }

    id = lookup->get<int>("channel", -1);

    on_ready();
}

void Publisher::send_callback(const SharedMessage, int state) {

    if (state != MESSAGE_CALLBACK_SENT && state != MESSAGE_CALLBACK_DROPPED)
        return;

    pending--;

}

void Publisher::on_ready() {

}

Publisher::Publisher(SharedClient client, const string &alias, const string &type, int queue, ssize_t chunk_size) :
    client(client), queue(queue), chunk_size(chunk_size) {

    identifier_generator = std::bind(std::uniform_int_distribution<int64_t> {}, std::mt19937(std::random_device {}()));

    using namespace std::placeholders;

    client->lookup_channel(alias, type, bind(&Publisher::lookup_callback, this, alias, _1));
}

Publisher::~Publisher() {


}

int Publisher::get_channel_id() {
    return id;
}


bool Publisher::send_message(uchar* data, int length) {

    if (id <= 0) return false;

    shared_ptr<Message> msg = make_shared<BufferedMessage>(data, length);
    MessageHandler::set_channel(msg, id);
    return send_message_internal(msg);

}

bool Publisher::send_message(MessageWriter& writer) {

    if (id <= 0) return false;

    shared_ptr<Message> msg = make_shared<BufferedMessage>(writer);
    MessageHandler::set_channel(msg, id);
    return send_message_internal(msg);

}

bool Publisher::send_message_internal(SharedMessage message) {

    //cout << pending << endl;

    if (queue > 0 && pending >= queue)
        return false;

    if (id <= 0) return false;

    using namespace std::placeholders;

    ssize_t length = message->get_length();

    pending++;

    if (length > chunk_size) {

        int chunks = (int) ceil((double) length / (double) chunk_size);
        int position = 0;

        int64_t identifier = identifier_generator();

        for (int i = 0; i < chunks; i++) {

            shared_ptr<MemoryBuffer> header = make_shared<MemoryBuffer>((i == 0 ? 2 : 1) * (sizeof(int64_t) + sizeof(int32_t)));
            MessageWriter writer(header->get_buffer(), header->get_length());
            writer.write_integer(i);
            writer.write_long(identifier);
            if (i == 0) {
                writer.write_long(length);
                writer.write_integer(chunk_size);
            }

            ssize_t clen = min(length - position, (ssize_t) chunk_size);

            vector<SharedBuffer> buffers;
            buffers.push_back(header);
            buffers.push_back(make_shared<ProxyBuffer>(message, position, clen));

            shared_ptr<Message> chunk = make_shared<MultiBufferMessage>(buffers);
            MessageHandler::set_channel(chunk, get_channel_id());

            if (i + 1 == chunks) {
                client->send(chunk, bind(&Publisher::send_callback, this, _1, _2));
            }
            else
                client->send(chunk);

            position += chunk_size;

        }

    } else {

        shared_ptr<MemoryBuffer> header = make_shared<MemoryBuffer>(sizeof(int));
        MessageWriter writer(header->get_buffer(), header->get_length());
        writer.write_integer(-1); // Negative number denotes a single chunk message

        vector<SharedBuffer> buffers;
        buffers.push_back(header);
        buffers.push_back(message);

        shared_ptr<Message> chunk = make_shared<MultiBufferMessage>(buffers);

        MessageHandler::set_channel(chunk, get_channel_id());

        client->send(chunk, bind(&Publisher::send_callback, this, _1, _2));

    }

    return true;
}

Publisher::ProxyBuffer::ProxyBuffer(SharedMessage parent, ssize_t start, ssize_t length) :
    parent(parent), start(start), length(length) {

}

Publisher::ProxyBuffer::~ProxyBuffer() {

}

ssize_t Publisher::ProxyBuffer::get_length() const {
    return length;
}

ssize_t Publisher::ProxyBuffer::copy_data(ssize_t position, uchar* buffer, ssize_t length) const {

    length = min(length, this->length - position);

    if (length < 1) return 0;

    return parent->copy_data(position + start, buffer, length);

}

SubscriptionWatcher::SubscriptionWatcher(SharedClient client, const string &alias, function<void(int)> callback) : 
    Watcher(client, alias), callback(callback), subscribers(0) {

}

SubscriptionWatcher::~SubscriptionWatcher() {

}

void SubscriptionWatcher::on_event(SharedDictionary message) {

    string type = message->get<string>("type", "");

    if (type == "subscribe" || type == "unsubscribe" || type == "summary") {
        subscribers = message->get<int>("subscribers", 0);

        if (callback)
            callback(subscribers);
    }

}

int SubscriptionWatcher::get_subscribers() const {
    return subscribers;
}

}





