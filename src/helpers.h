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

#endif

