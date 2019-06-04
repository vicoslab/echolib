/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_DATATYPES_HPP_
#define ECHO_DATATYPES_HPP_

#include <chrono>

#include <echolib/client.h>
#include <echolib/message.h>

namespace echolib {

template<typename T> string get_type_identifier();

template <typename T>
class TypedSubscriber : Subscriber {
  public:
    TypedSubscriber(SharedClient client, const string &alias, function<void(shared_ptr<T>)> callback) : Subscriber(client, alias, get_type_identifier<T>()), callback(callback) {

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

template <typename T>
class TypedPublisher : Publisher {
  public:
    TypedPublisher(SharedClient client, const string &alias, int queue = -1) : Publisher(client, alias, get_type_identifier<T>(), queue) {

    }

    virtual ~TypedPublisher() {

    }

    bool send(const T &data) {

        return Publisher::send_message<T>(data);

    }

};

template<typename T> using SharedTypedSubscriber = shared_ptr<TypedSubscriber<T> >;
template<typename T> using SharedTypedPublisher = shared_ptr<TypedPublisher<T> >;

template <> 
inline void read(MessageReader& reader, std::chrono::system_clock::time_point& timepoint) {
    
    std::chrono::microseconds raw((uint64_t) reader.read_long());

    timepoint = std::chrono::system_clock::time_point(raw);

}

template <> 
inline void write(MessageWriter& writer, const std::chrono::system_clock::time_point& timepoint) {

    uint64_t microseconds = std::chrono::time_point_cast<std::chrono::microseconds>(timepoint).time_since_epoch().count();

    writer.write_long((int64_t) microseconds);

}

class Header {
public:
    Header(std::string source = "", std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now());
    virtual ~Header();

    std::string source;
    std::chrono::system_clock::time_point timestamp;
};

template <> 
inline void read(MessageReader& reader, Header& header) {
    
    read(reader, header.source);
    read(reader, header.timestamp);

}

template <> 
inline void write(MessageWriter& writer, const Header& header) {

    write(writer, header.source);
    write(writer, header.timestamp);

}



template <> inline string get_type_identifier<Dictionary>() { return string("dictionary"); }

template<>
inline shared_ptr<Dictionary> Message::unpack(SharedMessage message) {

    MessageReader reader(message);

    shared_ptr<echolib::Dictionary> dictionary(new echolib::Dictionary);

    read(reader, *dictionary);

    return dictionary;
}

template<>
inline shared_ptr<Message> Message::pack(const Dictionary &data) {
    std::map<std::string, std::string>::const_iterator iter;

    ssize_t length = message_length(data);

    MessageWriter writer(length);

    write(writer, data);

    return make_shared<BufferedMessage>(writer);
}


}


#endif
