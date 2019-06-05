/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_CLIENT_HPP_
#define ECHO_CLIENT_HPP_

#include <string>
#include <map>
#include <set>
#include <functional>
#include <utility>
#include <mutex>
#include <type_traits>

#include "loop.h"
#include "message.h"

using namespace std;

namespace std {

template<typename Callback, typename Function> inline
bool func_compare(const Function &lhs, const Function &rhs) {
    typedef typename conditional
    <
    is_function<Callback>::value,
                typename add_pointer<Callback>::type,
                Callback
                >::type request_type;

    if (const request_type *lhs_internal = lhs.template target<request_type>())
        if (const request_type *rhs_internal = rhs.template target<request_type>())
            return *rhs_internal == *lhs_internal;
    return false;
}

template<typename function_signature>
struct function_comparable: function<function_signature> {
    typedef function<function_signature> Function;
    bool (*type_holder)(const Function &, const Function &);
public:
    function_comparable() {}
    template<typename Func> function_comparable(Func f)
        : Function(f), type_holder(func_compare<Func, Function>) {
    }
    template<typename Func> function_comparable &operator=(Func f) {
        Function::operator=(f);
        type_holder = func_compare<Func, Function>;
        return *this;
    }
    friend bool operator==(const Function &lhs, const function_comparable &rhs) {
        return rhs.type_holder(lhs, rhs);
    }
    friend bool operator==(const function_comparable &lhs, const Function &rhs) {
        return rhs == lhs;
    }
    friend void swap(function_comparable &lhs, function_comparable &rhs) { // noexcept
        lhs.swap(rhs);
        lhs.type_holder.swap(rhs.type_holder);
    }
};

}

namespace echolib {

class Client;
class Subscriber;
class Publisher;
class Watcher;
typedef std::shared_ptr<Subscriber> SharedSubscriber;
typedef std::shared_ptr<Publisher> SharedPublisher;
typedef std::shared_ptr<Watcher> SharedWatcher;
typedef std::shared_ptr<Client> SharedClient;

typedef std::shared_ptr<std::function<void(SharedDictionary)> > WatchCallback;
typedef std::shared_ptr<std::function<void(SharedMessage)> > DataCallback;

template<class F> DataCallback create_data_callback(F f) {
    return DataCallback(new std::function<void(SharedMessage)>(f));
}

template<class F> WatchCallback create_watch_callback(F f) {
    return WatchCallback(new std::function<void(SharedDictionary)>(f));
}

class Client : public IOBase, public MessageHandler {
    friend Subscriber;
    friend Publisher;
    friend Watcher;
public:

    Client(const string& name = "", const string& address = "");
    virtual ~Client();

    virtual bool handle_input();

    virtual bool handle_output();

    bool is_connected();

    virtual void disconnect();

    int get_queue_size();

    virtual int get_file_descriptor();

protected:

    bool unsubscribe(int channel, DataCallback callback);
    bool subscribe(int channel, DataCallback callback);
    bool watch(int channel, WatchCallback callback);
    bool unwatch(int channel, WatchCallback callback);
    void send(SharedMessage message, MessageCallback callback = NULL, int priority = 0);
    void lookup_channel(const string &alias, const string &type, function<void(SharedDictionary)> callback, bool create = true);

private:

    static const int TYPE_LOCAL;
    static const int TYPE_INET;

    void initialize_common();

    void send_command(SharedDictionary command, function<bool(SharedDictionary, SharedDictionary)> callback = NULL);

    bool handle_subscribe_response (SharedDictionary sent, SharedDictionary received);
    void handle_message(SharedMessage &message);

    int fd;
    bool connected;
    StreamWriter writer;
    StreamReader reader;

    int next_request_key;

    map<int, pair<SharedDictionary, function<bool(SharedDictionary, SharedDictionary)> > > requests;

    map<int, set<DataCallback> > subscriptions;
    map<int, set<WatchCallback> > watches;

    map<string, string> mappings;

    std::recursive_mutex mutex;

};

SharedClient connect(const string& socket = string(), const string& name = string(), SharedIOLoop loop = default_loop());

class Subscriber : public MessageHandler {
    friend Client;
public:
    Subscriber(SharedClient client, const string &alias, const string &type = string(), DataCallback callback = NULL, int pending_capacity = 10);

    virtual ~Subscriber();

    virtual void on_message(SharedMessage message);

    virtual void on_error(const std::exception& error);

    bool subscribe();

    bool unsubscribe();

protected:

    virtual void on_ready();

private:
    DataCallback internal_callback;

    DataCallback callback;

    void lookup_callback(SharedDictionary lookup);

    void data_callback(SharedMessage message);

    SharedClient client;
    int id = -1;

    class ChunkList : public vector<SharedMessage> {
    public:
        ChunkList(int length, int chunk_size);

        virtual ~ChunkList();

        bool set_chunk(int index, SharedMessage &message);

        bool is_complete() const;

        int chunks() const;

    private:

        int chunk_size;

    };

    int pending_capacity;

    map<int64_t, shared_ptr<ChunkList> > pending;


};

class Watcher {
public:
    Watcher(SharedClient client, const string &alias);

    virtual ~Watcher();

    virtual void on_event(SharedDictionary message) = 0;

    virtual void on_error(const std::exception& error) {};

    bool watch();

    bool unwatch();

protected:

    virtual void on_ready();

private:
    WatchCallback callback;

    void lookup_callback(SharedDictionary lookup);

    SharedClient client;
    int id = -1;

};

#define DEFAULT_CHUNK_SIZE 10 * 1024

class Publisher : public MessageHandler {
    friend Client;
public:
    Publisher(SharedClient client, const string &alias, const string &type = string(), int queue = -1, ssize_t chunk_size = DEFAULT_CHUNK_SIZE);

    virtual ~Publisher();

    bool send_message(uchar* data, int length);

    bool send_message(MessageWriter& writer);

protected:

    virtual void on_ready();

    int get_channel_id();

    template<typename T> bool send_message(const T &data) {

        if (get_channel_id() <= 0)
            return false;

        SharedMessage message = Message::pack<T>(data);
        MessageHandler::set_channel(message, get_channel_id());

        return send_message_internal(message);

    }

    virtual bool send_message_internal(SharedMessage message);

private:

    void lookup_callback(const string alias, SharedDictionary lookup);

    void send_callback(const SharedMessage message, int state);

    SharedClient client;
    int id = -1;
    int queue;

    int pending = 0;

    class ProxyBuffer : public Buffer {
    public:
        ProxyBuffer(SharedMessage parent, ssize_t start, ssize_t length);

        virtual ~ProxyBuffer();

        virtual ssize_t get_length() const;

        virtual ssize_t copy_data(ssize_t position, uchar* buffer, ssize_t length) const;

    private:

        SharedMessage parent;
        ssize_t start;
        ssize_t length;

    };

    ssize_t chunk_size;

    function<int64_t()> identifier_generator;

};

class SubscriptionWatcher : public Watcher {
public:
    SubscriptionWatcher(SharedClient client, const string &alias, function<void(int)> callback = NULL);

    virtual ~SubscriptionWatcher();

    virtual void on_event(SharedDictionary message);

    int get_subscribers() const;

private:
    function<void(int)> callback;

    int subscribers;

};

}

#endif
