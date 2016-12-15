/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_MESSAGE_HPP_
#define ECHO_MESSAGE_HPP_

#include <cstdio>
#include <string>
#include <cstring>
#include <sstream>
#include <map>
#include <queue>
#include <memory>
#include <exception>
#include <iostream>
#include <type_traits>

using namespace std;

#define ECHO_CONTROL_CHANNEL 0

// TODO: change this to strings
#define ECHO_COMMAND_UNKNOWN -4
#define ECHO_COMMAND_EVENT -3
#define ECHO_COMMAND_RESULT -2
#define ECHO_COMMAND_ERROR -1
#define ECHO_COMMAND_OK 0
#define ECHO_COMMAND_SUBSCRIBE 1
#define ECHO_COMMAND_UNSUBSCRIBE 2
#define ECHO_COMMAND_WATCH 3
#define ECHO_COMMAND_UNWATCH 4
#define ECHO_COMMAND_LOOKUP 5
#define ECHO_COMMAND_SUBSCRIBE_ALIAS 6
#define ECHO_COMMAND_CREATE_CHANNEL_WITH_ALIAS 7
#define ECHO_COMMAND_CREATE_CHANNEL 8
namespace echolib {

class StreamReader;
class StreamWriter;

typedef unsigned char uchar;

class MessageReader;
class MessageWriter;

class Message;

typedef shared_ptr<Message> SharedMessage;

class Buffer {
public:

    virtual ssize_t get_length() const = 0;

    virtual ssize_t copy_data(ssize_t position, uchar* buffer, ssize_t length) const = 0;

};

class MemoryBuffer : public virtual Buffer {
public:
    MemoryBuffer(ssize_t length);

    MemoryBuffer(MessageWriter &writer);

    virtual ~MemoryBuffer();

    virtual ssize_t get_length() const;

    virtual ssize_t copy_data(ssize_t position, uchar* buffer, ssize_t length) const;

    uchar* get_buffer() const;

protected:

    MemoryBuffer(uchar *data, ssize_t length, bool owned = true);

private:

    uchar *data;

    ssize_t data_length;

    bool data_owned;

};

typedef shared_ptr<Buffer> SharedBuffer;

class MessageHandler {
public:
    virtual ~MessageHandler() {};

protected:
    static void set_channel(SharedMessage message, int channel_id);
};

class Message : public std::enable_shared_from_this<Message>, public virtual Buffer {
    friend StreamReader;
    friend StreamWriter;
    friend MessageHandler;
public:

    /**
     *
     */
    Message();

    /**
     *
     */
    Message(int channel);

    /**
     *
     */
    virtual ~Message();

    /**
     * @return channel id
     */
    int get_channel() const;

    template<class T> static shared_ptr<Message> pack(const T &data);
    template<typename T> static shared_ptr<T> unpack(SharedMessage message);

protected:

    void set_channel(int channel);

private:

    int message_channel;

};

/**
 * Message class
 */
class BufferedMessage : public Message, virtual public MemoryBuffer {
public:

    BufferedMessage(uchar *data, ssize_t length, bool owned = true);

    BufferedMessage(MessageWriter& writer);

    BufferedMessage(int length);

    virtual ~BufferedMessage();

    using MemoryBuffer::get_length;

    using MemoryBuffer::copy_data;

private:

    uchar *data;
    ssize_t data_length;
    bool data_owned;

};

class MultiBufferMessage : public Message {
public:

    MultiBufferMessage(const vector<SharedBuffer> &buffers);

    virtual ~MultiBufferMessage();

    virtual ssize_t get_length() const;

    virtual ssize_t copy_data(ssize_t position, uchar* buffer, ssize_t length) const;

private:

    vector<SharedBuffer> buffers;
    vector<ssize_t> offsets;
    ssize_t length;

};

class EndOfBufferException: public std::exception {
    virtual const char* what() const throw();
};

class ParseException: public std::exception {


};

class MessageReader {
public:
    MessageReader(SharedMessage message);

    /**
     *
     */
    virtual ~MessageReader();

    template<typename T> T read() {
        static_assert(std::is_arithmetic<T>::value, "Only primitive numeric types supported");
        T value;
        copy_data((uchar *) &value, sizeof(T));
        return value;
    }

    /**
     * @return next integer
     */
    int read_integer();

    /**
     * @return next long integer
     */
    long read_long();

    /**
     * @return next Boolean value
     */
    bool read_bool();

    /**
     * @return next character
     */
    char read_char();

    /**
     * @return next character
     */
    double read_double();

    /**
     * @return next character
     */
    float read_float();

    /**
     * @return next string
     */
    string read_string();

    ssize_t get_position() const;

    ssize_t get_length() const;

    void copy_data(uchar* buffer, ssize_t length = 0);

private:

    SharedMessage message;

    ssize_t position;

};

template<> inline string MessageReader::read<string>() {
    return read_string();
}

template<> inline bool MessageReader::read<bool>() {
    return read_bool();
}

template<typename T> void read(MessageReader& reader, T& dst) {
    static_assert(std::is_arithmetic<T>::value, "Only primitive numeric types supported");
    dst = reader.read<T>();
}

template<> inline void read(MessageReader& reader, string& dst) {
    dst = reader.read_string();
}

template<typename T> void read(MessageReader& reader, vector<T>& dst) {
    int n = reader.read<int>();
    dst.resize(n);
    for (size_t i = 0; i < n; i++) {
        read(reader, dst[i]);
    }
}

class MessageWriter {
    friend MemoryBuffer;
    friend MessageReader;
public:
    /**
     *
     */
    ~MessageWriter();

    MessageWriter(ssize_t length = 0);

    MessageWriter(uchar* buffer, ssize_t length);

    template<typename T> void write(const T& value) {

        static_assert(std::is_arithmetic<T>::value, "Only primitive numeric types supported here");
        write_buffer((uchar *)&value, sizeof(T));

    }

    int write_integer(int value);

    int write_long(long value);

    int write_bool(bool value);

    int write_char(char value);

    int write_float(float value);

    int write_double(double value);

    int write_string(const string& value);

    int write_buffer(const uchar* buffer, ssize_t len);

    int write_buffer(MessageReader& reader, ssize_t len);

    ssize_t get_length();

private:


    bool data_owned;
    uchar *data;
    ssize_t data_length;
    ssize_t data_position;

};

template<> inline void MessageWriter::write<string>(const string& value) {
    write_string(value);
}

template<> inline void MessageWriter::write<bool>(const bool& value) {
    write_bool(value);
}

template<typename T> void write(MessageWriter& writer, const T& src) {
    static_assert(std::is_arithmetic<T>::value, "Only primitive numeric types supported here");
    writer.write<T>(src);
}

template<> inline void write(MessageWriter& writer, const string& src) {
    writer.write_string(src);
}

template<typename T> void write(MessageWriter& writer, const vector<T>& src) {
    writer.write<int>((int)src.size());
    
    for (size_t i = 0; i < src.size(); i++) {
        write(writer, src[i]);
    }
}

typedef std::map<std::string, std::string>::const_iterator DictionaryIterator;

class Dictionary : public std::enable_shared_from_this<Dictionary> {
    friend Message;
public:

    Dictionary();
    ~Dictionary();

    template<class T> void set(const string &key, const T &value );
    template<class T> T get( const string &key ) const;
    template<class T> T get( const string &key, const T &value ) const;

    bool contains(const string key) const;

    size_t size() const;

    DictionaryIterator begin() const;
    DictionaryIterator end() const;

protected:

    template<class T> static string T_as_string(const  T &t );
    template<class T> static T string_as_T( const string &s );
    static void trim( string &s );

private:

    int code;

    bool valid_argument_name(const string &key);
    map<string, string> arguments;

};

typedef shared_ptr<Dictionary> SharedDictionary;

inline SharedDictionary generate_command(int code) {

    SharedDictionary command = make_shared<Dictionary>();
    command->set<int>("code", code);
    return command;

}

class StreamReader {
public:

    StreamReader(int fd);
    ~StreamReader();

    SharedMessage read_message();

    int get_error();

    //todo: za test
    int fd;
private:



    void reset();

    SharedMessage process_buffer();

    int error;

    int state;
    int header_value;

    int message_channel;
    uchar *data;
    int data_length;
    int data_current;
    long total_data_read;

    uchar *buffer;
    ssize_t buffer_length;
    ssize_t buffer_position;

};

class StreamWriter {
public:

    StreamWriter(int fd);
    ~StreamWriter();

    void add_message(const SharedMessage msg, int priority);

    bool write_messages();

    int get_error();

    int get_queue_size();

protected:

    class MessageContainer {
    public:
        MessageContainer() : priority(0), time(0) {}
        MessageContainer(SharedMessage message, int priority, long time) : message(message), priority(priority), time(time) {}
        SharedMessage message;
        int priority;
        long time;

        bool is_empty() const {
            return !message;
        }
        void reset() {
            message.reset();
        }
    };

    static bool comparator(const MessageContainer &lhs, const MessageContainer &rhs);


private:

    int fd;

    void reset();

    bool process_message();

    int error;

    priority_queue<MessageContainer, vector<MessageContainer>, function<bool(MessageContainer, MessageContainer)> > incoming;

    MessageContainer pending;

    uchar *buffer;
    ssize_t buffer_position;
    ssize_t buffer_length;

    int state;
    ssize_t message_position;
    ssize_t message_length;

    unsigned long time;

};

template<class T>
string Dictionary::T_as_string(const T &t ) {
    // Convert from a T to a string
    // Type T must support << operator
    std::ostringstream ost;
    ost << t;
    string ret = ost.str();
    return ret;
}

template<class T>
T Dictionary::string_as_T( const string &s ) {
    // Convert from a string to a T
    // Type T must support >> operator
    T t;
    std::istringstream ist(s);
    ist >> t;
    return t;
}

template<>
inline string Dictionary::string_as_T<string>( const string &s ) {
    // Convert from a string to a string
    // In other words, do nothing
    return s;
}

template<>
inline bool Dictionary::string_as_T<bool>( const string &s ) {
    // Convert from a string to a bool
    // Interpret "false", "F", "no", "n", "0" as false
    // Interpret "true", "T", "yes", "y", "1", "-1", or anything else as true
    bool b = true;
    string sup = s;
    for ( string::iterator p = sup.begin(); p != sup.end(); ++p )
        *p = toupper(*p);  // make string all caps
    if ( sup == string("FALSE") || sup == string("F") ||
            sup == string("NO") || sup == string("N") ||
            sup == string("0") || sup == string("NONE") )
        b = false;
    return b;
}

template<class T>
T Dictionary::get( const string &key ) const {
    std::map<std::string, std::string>::const_iterator it = arguments.find(key);

    if (it != arguments.end())
        return string_as_T<T>( it->second );

    return string_as_T<T>(string());

}

template<class T>
T Dictionary::get( const string &key, const T &def ) const {
    std::map<std::string, std::string>::const_iterator it = arguments.find(key);

    if (it == arguments.end()) {
        return def;
    }

    return string_as_T<T>( it->second );
}

template<class T>
void Dictionary::set(const string &key, const T &value ) {
    if (valid_argument_name(key)) {
        // TODO: segfault here?
        string v = T_as_string( value );
        arguments[key] = v;

    }

}

}

#include <type_traits>

namespace echolib {

template<>
inline void read(MessageReader& reader, Dictionary& dictionary) {

    int n = reader.read_integer();
    for (int i = 0; i < n; i++) {
        const std::string key = reader.read_string();
        const std::string value = reader.read_string();
        dictionary.set(key, value);
    }

}

template<>
inline void write(MessageWriter& writer, const Dictionary &data) {
    DictionaryIterator iter;

    writer.write_integer(data.size());

    for (iter = data.begin(); iter != data.end(); ++iter) {
        writer.write_string(iter->first);
        writer.write_string(iter->second);
    }

}


//template <> inline string get_type_identifier<Dictionary>() { return string("dictionary"); }

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

    int length = 0;

    for (iter = data.arguments.begin(); iter != data.arguments.end(); ++iter) {
        length += sizeof(int) * 4 + sizeof(char) * (iter->first.size() + iter->second.size());
    }

    MessageWriter writer(length);

    write(writer, data);

    return make_shared<BufferedMessage>(writer);
}

}

#endif
