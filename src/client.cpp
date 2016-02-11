/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <stdexcept>
#include <exception>
#include <arpa/inet.h>
#include <cmath>
#include <random>
#include <chrono>
#include <netinet/in.h>

#include "debug.h"
#include "message.h"
#include "client.h"

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

static int connect_socket(const string &path) {

    int len, fd;
    struct sockaddr_un remote;

    if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) == -1) {
        throw runtime_error("Unable to create socket");
    }

    remote.sun_family = AF_UNIX;
    strcpy(remote.sun_path, path.c_str());

    len = strlen(remote.sun_path) + sizeof(remote.sun_family);
    if (connect(fd, (struct sockaddr *)&remote, len) == -1) {
        throw runtime_error("Unable to connect socket");
    }

    if (!make_socket_non_blocking(fd)) {
        throw runtime_error("Unable to set socket to non-blocking mode");
    }

    return fd;
}


const int Client::TYPE_INET=1;
const int Client::TYPE_LOCAL=0;


Client::Client(const string &path) : fd(connect_socket(path)), writer(fd), reader(fd),
    next_request_key(0), subscriptions(), watches() {
    __debug_enable();


    struct epoll_event event;
    efd = epoll_create1 (0);
    if (efd == -1) {
        throw runtime_error("Unable to use epoll");
    }

    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLOUT;
    if (epoll_ctl (efd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw runtime_error("Unable to use epoll");
    }

    connected = true;

}


static int connect_socket(const string &path, int port) {
    printf("Typed connect socket\n");
    printf ("IP: %s, Port: %d",path.c_str(),port);
    int fd;
    struct sockaddr_in remote;
    printf("Type: Internet socket\n");

    if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        throw runtime_error("Unable to create socket");
    }

    remote.sin_family = AF_INET;
    inet_pton(AF_INET,path.c_str(),&(remote.sin_addr));
    remote.sin_port=htons(port);
    if (connect(fd, (struct sockaddr *) &remote, sizeof(remote)) == -1) {
        perror("connect");
        throw runtime_error("Unable to connect socket");
    }

    if (!make_socket_non_blocking(fd)) {
        throw runtime_error("Unable to set socket to non-blocking mode");
    }
    return fd;
}



Client::Client(const string &path, int port) : fd(connect_socket(path,port)), writer(fd), reader(fd),
    next_request_key(0), subscriptions(), watches() {
    __debug_enable();


    struct epoll_event event;
    efd = epoll_create1 (0);
    if (efd == -1) {
        throw runtime_error("Unable to use epoll");
    }

    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET | EPOLLOUT;
    if (epoll_ctl (efd, EPOLL_CTL_ADD, fd, &event) == -1) {
        throw runtime_error("Unable to use epoll");
    }

    connected = true;

}

Client::~Client() {

    disconnect();

}

bool Client::wait(long timeout) {
    SYNCHRONIZED(mutex);

    struct epoll_event *events;
    events = (epoll_event *) calloc (MAXEVENTS, sizeof(epoll_event));

    auto start = std::chrono::system_clock::now();
    while (true) {
        auto current = std::chrono::system_clock::now();

        auto duration= std::chrono::duration_cast<std::chrono::milliseconds>(current-start);
        long remaining = timeout - (duration.count());
        if (remaining < 1)
            break;

        int n = epoll_wait (efd, events, MAXEVENTS, remaining);
        for (int i = 0; i < n; i++) {
            if ((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP)) {
                perror("epoll");
                disconnect();
                free(events);
                return is_connected();
            }
        }


        // Reading phase
        // TODO: handle timeout
        while (true) {
            SharedMessage msg = reader.read_message();

            if (!msg) {
                if (reader.get_error()) {
                    disconnect();
                    free(events);
                    return is_connected();
                }
                break;
            } else {
                handle_message(msg);
            }

        }

        //Writing phase
        if (!writer.write_messages()) {
            if (writer.get_error()) {
                DEBUGMSG("Writer error: %d\n", writer.get_error());
                disconnect();
                free(events);
                return is_connected();
            }
        }

    }

    free(events);
    return is_connected();

}

int Client::get_queue_size() {
    return writer.get_queue_size();
}

bool Client::is_connected() {

    return connected;
}

bool Client::disconnect() {
    SYNCHRONIZED(mutex);

    if (is_connected()) {
        DEBUGMSG("Disconnecting.\n");
        close(fd);
    }

    connected = false;
    return true;

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

    if(subscriptions.find(channel) == subscriptions.end()) {
        DEBUGMSG("Subscribing to channel %d\n", channel);
        //Generate a subscription command message
        SharedDictionary command = generate_command(ECHO_COMMAND_SUBSCRIBE);
        command->set<int>("channel",channel);
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

    if(subscriptions.find(channel) == subscriptions.end()) {
        DEBUGMSG("Unknown channel %d\n", channel);
        return false;
    }

    if(subscriptions[channel].find(callback) == subscriptions[channel].end()) {
        DEBUGMSG("Unknown callback for channel %d\n", channel);
        return false;
    }

    if (!subscriptions[channel].erase(callback))
        return false;

    if(subscriptions[channel].size() == 0) {
        DEBUGMSG("No more subscribers for %d\n", channel);
        //no more callbacks, we can unsubscribe
        SharedDictionary command = generate_command(ECHO_COMMAND_UNSUBSCRIBE);
        command->set<int>("channel",channel);
        std::function<bool(SharedDictionary,SharedDictionary)>callback = [](SharedDictionary x, SharedDictionary y) {
            return true;
        };
        //add the unsubscribe command to message queue
        send_command(command,callback);
        subscriptions.erase(channel);
    }

    return true;
}

bool Client::watch(int channel, WatchCallback callback) {
    SYNCHRONIZED(mutex);

    if(watches.find(channel) == watches.end()) {
        //Generate a subscription command message
        SharedDictionary command = generate_command(ECHO_COMMAND_WATCH);
        command->set<int>("channel",channel);
        std::function<bool(SharedDictionary,SharedDictionary)>callback = [](SharedDictionary x, SharedDictionary y) {
            return true;
        };
        //add the watch command to message queue
        send_command(command,callback);
    }

    return watches[channel].insert(callback).second;  // Returns pair, the second value is success
}

bool Client::unwatch(int channel, WatchCallback callback) {
    SYNCHRONIZED(mutex);

    if(watches.find(channel) == watches.end()) {
        DEBUGMSG("Unknown channel %d\n", channel);
        return false;
    }

    if(watches[channel].find(callback) == watches[channel].end()) {
        DEBUGMSG("Unknown callback for channel %d\n", channel);
        return false;
    }

    if (!watches[channel].erase(callback))
        return false;

    if(watches[channel].size() == 0) {
        DEBUGMSG("No more watchers for %d\n", channel);
        // No more callbacks, we can unwatch
        SharedDictionary command = generate_command(ECHO_COMMAND_UNWATCH);
        command->set<int>("channel",channel);
        std::function<bool(SharedDictionary,SharedDictionary)>callback = [](SharedDictionary x, SharedDictionary y) {
            return true;
        };
        //add the unwatch command to message queue
        send_command(command,callback);
        watches.erase(channel);
    }

    return true;
}

void Client::send(SharedMessage message) {

    SYNCHRONIZED(mutex);

    if (!is_connected()) return;

    writer.add_message(message, 0);

}

void Client::send_command(SharedDictionary command, function<bool(SharedDictionary, SharedDictionary)> callback) {

    SYNCHRONIZED(mutex);

    int key = next_request_key++;
    command->set<int>("key", key);

    pair<SharedDictionary, function<bool(SharedDictionary, SharedDictionary)> > pending(command, callback);
    requests[key] = pending;
    send(Message::pack<Dictionary>(ECHO_CONTROL_CHANNEL, *command));

}

static bool internal_lookup_callback(SharedDictionary in, SharedDictionary out, function<void(SharedDictionary)> callback) {
    callback(out);
    return true;
}

void Client::lookup_channel(const string &alias, const string &type, function<void(SharedDictionary)> callback, bool create) {

    using namespace std::placeholders;

    SYNCHRONIZED(mutex);

    // Create appropriate command
    SharedDictionary command = generate_command(ECHO_COMMAND_LOOKUP);
    command->set<string>("alias", alias);
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

Subscriber::Subscriber(SharedClient client, const string &alias, const string &type) :
    Subscriber(client, alias, type, create_data_callback(bind(&Subscriber::on_message, this, std::placeholders::_1))) {

}

Subscriber::Subscriber(SharedClient client, const string &alias, const string &type, DataCallback callback) : callback(callback), client(client) {

    using namespace std::placeholders;

    client->lookup_channel(alias, type, bind(&Subscriber::lookup_callback, this, _1));

}


Subscriber::~Subscriber() {

    unsubscribe();

}

bool Subscriber::subscribe() {
    if (this->id < 1) return false;
    return client->subscribe(id, callback);
}

bool Subscriber::unsubscribe() {
    if (this->id < 1) return false;
    return client->unsubscribe(id, callback);
}

void Subscriber::on_ready() {

}

ChunkedSubscriber::ChunkedSubscriber(SharedClient client, const string &alias, const string &type) :
    Subscriber(client, alias, string("chunked ") + type, create_data_callback(bind(&ChunkedSubscriber::on_chunk, this, std::placeholders::_1))) {

}

ChunkedSubscriber::~ChunkedSubscriber() {

}

void ChunkedSubscriber::on_chunk(SharedMessage chunk) {

    MessageReader reader(chunk);

    long id = reader.read_long();
    int sequence = reader.read_integer();

    bool valid = true;

    if (pending.find(id) != pending.end()) {

        valid = pending[id]->set_chunk(sequence, reader);

    } else {

        if (sequence != 0) return;

        long length = reader.read_long();
        int chunk_size = reader.read_integer();
        pending[id] = make_shared<PendingBuffer>(length, chunk_size);
        valid = pending[id]->set_chunk(sequence, reader);

    }

    MemoryBuffer buffer(chunk->get_length());
    chunk->copy_data(0, buffer.get_buffer(), buffer.get_length());

    string tmp((char*)buffer.get_buffer(), buffer.get_length());

    if (!valid) {

        pending.erase(id);
        return;

    }

    if (pending[id]->is_complete()) {
        shared_ptr<PendingBuffer> buffer = pending[id];
        pending.erase(id);
        on_message(make_shared<BufferedMessage>(chunk->get_channel(), *buffer));
    }

}

ChunkedSubscriber::PendingBuffer::PendingBuffer(int length, int chunk_size) : MessageWriter(length), chunk_size(chunk_size) {
    int chunks = (int) ceil((double) length / (double) chunk_size);

    chunk_present.resize(chunks, false);

}

ChunkedSubscriber::PendingBuffer::~PendingBuffer() {


}

bool ChunkedSubscriber::PendingBuffer::set_chunk(int index, MessageReader& reader) {

    ssize_t length = reader.get_length() - reader.get_position();

    if (index > 0 && !chunk_present[index-1])
        return false;

    write_buffer(reader, length);

    chunk_present[index] = true;

    return true;

}

bool ChunkedSubscriber::PendingBuffer::is_complete() const {

    return chunk_present[chunk_present.size()-1];

}

int ChunkedSubscriber::PendingBuffer::chunks() const {

    return chunk_present.size();

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

void Publisher::lookup_callback(SharedDictionary lookup) {

    if (lookup->contains("error")) {
        DEBUGMSG("Publisher error: %s\n", lookup->get<string>("error").c_str());
        return;
    }

    id = lookup->get<int>("channel", -1);

    on_ready();
}

void Publisher::on_ready() {

}

Publisher::Publisher(SharedClient client, const string &alias, const string &type) : client(client) {

    using namespace std::placeholders;

    client->lookup_channel(alias, type, bind(&Publisher::lookup_callback, this, _1));
}

Publisher::~Publisher() {


}

int Publisher::get_channel_id() {
    return id;
}


bool Publisher::send_message(uchar* data, int length) {

    if (id <= 0) return false;

    return send_message_internal(make_shared<BufferedMessage>(id, data, length));

}

bool Publisher::send_message(MessageWriter& writer) {

    if (id <= 0) return false;

    return send_message_internal(make_shared<BufferedMessage>(id, writer));

}

bool Publisher::send_message_internal(SharedMessage message) {

    if (id <= 0) return false;

    client->send(message);

    return true;
}

ChunkedPublisher::ChunkedPublisher(SharedClient client, const string &alias, const string &type, int chunk_size) : Publisher(client, alias, string("chunked ") + type), chunk_size(chunk_size) {

    identifier_generator = std::bind(std::uniform_int_distribution<long> {}, std::mt19937(std::random_device {}()));


}

ChunkedPublisher::~ChunkedPublisher() {


}

bool ChunkedPublisher::send_message_internal(SharedMessage message) {

    if (get_channel_id() <= 0) return false;

    ssize_t length = message->get_length();

    int chunks = (int) ceil((double) length / (double) chunk_size);
    int position = 0;

    long identifier = identifier_generator();

    for (int i = 0; i < chunks; i++) {

        shared_ptr<MemoryBuffer> header = make_shared<MemoryBuffer>((i == 0 ? 2 : 1) * (sizeof(long) + sizeof(int)));
        MessageWriter writer(header->get_buffer(), header->get_length());
        writer.write_long(identifier);
        writer.write_integer(i);
        if (i == 0) {
            writer.write_long(length);
            writer.write_integer(chunk_size);
        }

        ssize_t clen = min(length - position, (ssize_t) chunk_size);

        vector<SharedBuffer> buffers;
        buffers.push_back(header);
        buffers.push_back(make_shared<ProxyBuffer>(message, position, clen));

        shared_ptr<Message> chunk = make_shared<MultiBufferMessage>(get_channel_id(), buffers);

        Publisher::send_message_internal(chunk);

        position += chunk_size;

    }

    return true;
}

ChunkedPublisher::ProxyBuffer::ProxyBuffer(SharedMessage parent, ssize_t start, ssize_t length) :
    parent(parent), start(start), length(length) {

}

ChunkedPublisher::ProxyBuffer::~ProxyBuffer() {

}

ssize_t ChunkedPublisher::ProxyBuffer::get_length() const {
    return length;
}

ssize_t ChunkedPublisher::ProxyBuffer::copy_data(ssize_t position, uchar* buffer, ssize_t length) const {

    length = min(length, this->length - position);

    if (length < 1) return 0;

    return parent->copy_data(position + start, buffer, length);

}

SubscriptionWatcher::SubscriptionWatcher(SharedClient client, const string &alias, function<void(int)> callback) : Watcher(client, alias), callback(callback) {

}

void SubscriptionWatcher::on_event(SharedDictionary message) {

    string type = message->get<string>("type", "");

    if (type == "subscribe" || type == "unsubscribe" || type == "summary") {
        int subscribers = message->get<int>("subscribers", 0);
        callback(subscribers);
    }

}

}





