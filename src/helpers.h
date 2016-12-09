/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_HELPERS_HPP_
#define ECHO_HELPERS_HPP_

#include "client.h"
#include "datatypes.h"

namespace echolib {

template<typename T> class StaticPublisher: public TypedPublisher<T>, public Watcher, public std::enable_shared_from_this<StaticPublisher<T> > {
public:
    StaticPublisher(SharedClient client, const string &alias, T& value) : TypedPublisher<T>(client, alias), Watcher(client, alias), value(value) {

        subscribers = 0;

    }

    using TypedPublisher<T>::send;

    virtual ~StaticPublisher() {}

    virtual void on_event(SharedDictionary message) {
        string type = message->get<string>("type", "");

        if (type == "subscribe" || type == "unsubscribe" || type == "summary") {
            int s = message->get<int>("subscribers", 0);
            if (s > subscribers) {
                send(value);
            }
            subscribers = s;
        }
    }

private:

    int subscribers;

    T value;

};

}

// Optional protobuf support
#ifdef GOOGLE_PROTOBUF_VERSION

#include <google/protobuf/message_lite.h>
#include <google/protobuf/io/zero_copy_stream_impl_lite.h>

using namespace google::protobuf;
using namespace google::protobuf::io;

namespace echolib {

//template<class T> class std::enable_if<std::is_base_of<MessageLite, T>::value, T>::type;

template<class T> shared_ptr<T> Message::unpack(SharedMessage message) {

    static_assert(std::is_base_of<MessageLite, T>::value, "T must inherit from echo::protobuf::MessageLite");

    ArrayInputStream stream(data, data_length);

    //typename std::enable_if<std::is_base_of<MessageLite, T>::value, T>::type

    shared_ptr<T> parsed(make_shared<T>());
    parsed->ParseFromZeroCopyStream(&stream);

    return parsed;

}

template<class T>
inline shared_ptr<Message> Message::pack(int channel, const T &data) {

    static_assert(std::is_base_of<MessageLite, T>::value, "T must inherit from echo::protobuf::MessageLite");

    shared_ptr<BufferedMessage> message = make_shared<BufferedMessage>(channel, data.ByteSize());

    uchar *buffer = (uchar *) malloc(sizeof(char) * length);

    ArrayOutputStream stream(messsage->get_buffer(), message->get_length());

    data.SerializeToZeroCopyStream(&stream);

    return message;
}

}

#endif

// Optional protobuf support
#ifdef FLATBUFFERS_VERSION_MAJOR

#include <flatbuffers/flatbuffers.h>

using namespace flatbuffers;

namespace echolib {

template <typename T>
class FlatbuffersSubscriber : public Subscriber {
  public:
    TypedSubscriber(SharedClient client, const string &alias, function<void(shared_ptr<T>)> callback) : Subscriber(client, alias, TypeIdentifier<T>::name), callback(callback) {

    }

    virtual ~FlatbuffersSubscriber() {

    }

    virtual void on_message(SharedMessage message) {

        try {

            shared_ptr<T> data = (message);

            callback(data);

        } catch (echolib::ParseException &e) {
            Subscriber::on_error(e);
        }

    };

  private:
    function<void(shared_ptr<T>)> callback;

};

template <typename T, bool Chunked = false>
class TypedPublisher : public std::conditional<Chunked,ChunkedPublisher,Publisher>::type {
  public:
    TypedPublisher(SharedClient client, const string &alias) : std::conditional<Chunked,ChunkedPublisher,Publisher>::type(client, alias, TypeIdentifier<T>::name) {

    }

    virtual ~TypedPublisher() {

    }

    bool send(const T &data) {

        return Publisher::send_message<T>(data);

    }

};

//template<class T> class std::enable_if<std::is_base_of<MessageLite, T>::value, T>::type;

template<class T> shared_ptr<T> Message::unpack(SharedMessage message) {

    static_assert(std::is_base_of<MessageLite, T>::value, "T must inherit from echo::protobuf::MessageLite");

    ArrayInputStream stream(data, data_length);

    //typename std::enable_if<std::is_base_of<MessageLite, T>::value, T>::type

    shared_ptr<T> parsed(make_shared<T>());
    parsed->ParseFromZeroCopyStream(&stream);

    return parsed;

}

template<class T>
inline shared_ptr<Message> Message::pack(int channel, const T &data) {

    static_assert(std::is_base_of<MessageLite, T>::value, "T must inherit from echo::protobuf::MessageLite");

    shared_ptr<BufferedMessage> message = make_shared<BufferedMessage>(channel, data.ByteSize());

    uchar *buffer = (uchar *) malloc(sizeof(char) * length);

    ArrayOutputStream stream(messsage->get_buffer(), message->get_length());

    data.SerializeToZeroCopyStream(&stream);

    return message;
}

}

#endif

#endif

