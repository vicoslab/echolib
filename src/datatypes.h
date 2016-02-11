/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_DATATYPES_HPP_
#define ECHO_DATATYPES_HPP_

#include "client.h"
#include "message.h"

namespace echolib {

template<typename T> struct TypeIdentifier;

template <typename T, bool Chunked = false>
class TypedSubscriber : public std::conditional<Chunked,ChunkedSubscriber,Subscriber>::type {
  public:
    TypedSubscriber(SharedClient client, const string &alias, function<void(shared_ptr<T>)> callback) : std::conditional<Chunked,ChunkedSubscriber,Subscriber>::type(client, alias, TypeIdentifier<T>::name), callback(callback) {

    }

    virtual ~TypedSubscriber() {

    }

    virtual void on_message(SharedMessage message) {

        try {

            shared_ptr<T> data = Message::unpack<T>(message);

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

template<typename T> using SharedTypedSubscriber = shared_ptr<TypedSubscriber<T> >;
template<typename T> using SharedTypedPublisher = shared_ptr<TypedPublisher<T> >;

#define REGISTER_TYPE_IDENTIFIER(X) template <> struct TypeIdentifier< X > \
    { static const char* name; }; const char* TypeIdentifier< X >::name = #X

template <> struct TypeIdentifier<Dictionary> {
    static const char* name;
};
const char* TypeIdentifier<Dictionary>::name = "dictionary";

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

#endif
