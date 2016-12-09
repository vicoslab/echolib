/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_DATATYPES_HPP_
#define ECHO_DATATYPES_HPP_

#include "client.h"
#include "message.h"

namespace echolib {

template<typename T> string get_type_identifier();

template <typename T, bool Chunked = false>
class TypedSubscriber : public std::conditional<Chunked,ChunkedSubscriber,Subscriber>::type {
  public:
    TypedSubscriber(SharedClient client, const string &alias, function<void(shared_ptr<T>)> callback) : std::conditional<Chunked,ChunkedSubscriber,Subscriber>::type(client, alias, get_type_identifier<T>()), callback(callback) {

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
    TypedPublisher(SharedClient client, const string &alias) : std::conditional<Chunked,ChunkedPublisher,Publisher>::type(client, alias, get_type_identifier<T>()) {

    }

    virtual ~TypedPublisher() {

    }

    bool send(const T &data) {

        return Publisher::send_message<T>(data);

    }

};

template<typename T> using SharedTypedSubscriber = shared_ptr<TypedSubscriber<T> >;
template<typename T> using SharedTypedPublisher = shared_ptr<TypedPublisher<T> >;

}


#endif
