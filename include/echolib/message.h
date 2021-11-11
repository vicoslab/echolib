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
#include <functional>
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
#define ECHO_COMMAND_SET_NAME 9
#define ECHO_COMMAND_GET_NAME 10

//TODO: move buffer size from a define to a variable that the user can change, since it has an effect on performance
#define BUFFER_SIZE 1024 * 100
#define MESSAGE_MAX_SIZE 1024 * 50
#define MESSAGE_MAX_QUEUE 5000

namespace echolib {

template <bool B, typename T = void> using enable_if_t = typename std::enable_if<B, T>::type;

template <typename T, typename = void> struct is_input_iterator : std::false_type {};
template <typename T>
struct is_input_iterator<T, void_t<decltype(*std::declval<T &>()), decltype(++std::declval<T &>())>>
    : std::true_type {};

template <typename T>
class any_container {
    std::vector<T> v;
public:
    any_container() = default;

    // Can construct from a pair of iterators
    template <typename It, typename = enable_if_t<is_input_iterator<It>::value>>
    any_container(It first, It last) : v(first, last) { }

    // Implicit conversion constructor from any arbitrary container type with values convertible to T
    template <typename Container, typename = enable_if_t<std::is_convertible<decltype(*std::begin(std::declval<const Container &>())), T>::value>>
    any_container(const Container &c) : any_container(std::begin(c), std::end(c)) { }

    // initializer_list's aren't deducible, so don't get matched by the above template; we need this
    // to explicitly allow implicit conversion from one:
    template <typename TIn, typename = enable_if_t<std::is_convertible<TIn, T>::value>>
    any_container(const std::initializer_list<TIn> &c) : any_container(c.begin(), c.end()) { }

    // Avoid copying if given an rvalue vector of the correct type.
    any_container(std::vector<T> &&v) : v(std::move(v)) { }

    // Moves the vector out of an rvalue any_container
    operator std::vector<T> &&() && { return std::move(v); }

    // Dereferencing obtains a reference to the underlying vector
    std::vector<T> &operator*() { return v; }
    const std::vector<T> &operator*() const { return v; }

    // -> lets you call methods on the underlying vector
    std::vector<T> *operator->() { return &v; }
    const std::vector<T> *operator->() const { return &v; }

};

class StreamReader;
class StreamWriter;

typedef unsigned char uchar;

class MessageReader;
class MessageWriter;

class Message;

typedef shared_ptr<Message> SharedMessage;

class Buffer {
public:

    virtual size_t get_length() const = 0;

    virtual size_t copy_data(size_t position, uchar* buffer, size_t length) const = 0;

};

class MemoryBuffer : public virtual Buffer {
public:
    MemoryBuffer(size_t length);

    MemoryBuffer(MessageWriter &writer);

    virtual ~MemoryBuffer();

    virtual size_t get_length() const;

    virtual size_t copy_data(size_t position, uchar* buffer, size_t length) const;

    uchar* get_buffer() const;

protected:

    MemoryBuffer(uchar *data, size_t length, bool owned = true);

private:

    uchar *data;

    size_t data_length;

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

    BufferedMessage(uchar *data, size_t length, bool owned = true);

    BufferedMessage(MessageWriter& writer);

    BufferedMessage(int length);

    virtual ~BufferedMessage();

    using MemoryBuffer::get_length;

    using MemoryBuffer::copy_data;

private:

    uchar *data;
    size_t data_length;
    bool data_owned;

};

class MultiBufferMessage : public Message {
public:

    MultiBufferMessage(any_container<SharedBuffer> buffers);

    template <class I> MultiBufferMessage(I begin, I end) : buffers(begin, end) { rebuild(); }

    virtual ~MultiBufferMessage();

    virtual size_t get_length() const;

    virtual size_t copy_data(size_t position, uchar* buffer, size_t length) const;

private:

    void rebuild();

    MultiBufferMessage();

    vector<SharedBuffer> buffers;
    vector<size_t> offsets;
    size_t length;

};

template<class T>
class PrimitiveBuffer : public Buffer {
public:
    static_assert(std::is_fundamental<T>::value, "Only fundamental types allowed");

    PrimitiveBuffer(const T value) : value(value) { }

    virtual ~PrimitiveBuffer() { }

    virtual size_t get_length() const { return sizeof(T); }

    virtual size_t copy_data(size_t position, uchar* buffer, size_t length) const {

        length = min(length, sizeof(T) - position);
        if (length < 1) {
            return 0;
        } 

        memcpy(buffer, (void *) &(((uchar *) &(value))[position]), length);

        return length;
    }

    static SharedBuffer wrap(const T value) {
        return SharedBuffer(new PrimitiveBuffer<T>(value));
    }

private:

    T value;
    
};

template<class T>
class ListBuffer : public Buffer {
public:
    static_assert(std::is_fundamental<T>::value, "Only fundamental types allowed");

    ListBuffer(const any_container<T> value) : value(*value) { }

    virtual ~ListBuffer() { }

    virtual size_t get_length() const { return sizeof(size_t) + sizeof(T) * value.size(); }

    virtual size_t copy_data(size_t position, uchar* buffer, size_t length) const {

        length = min(length, get_length() - position);
        if (length < 1) return 0;

        size_t plength = 0;

        if (position < sizeof(size_t)) {
            size_t size = value.size();
            plength = min(sizeof(size_t) - position, length);
            memcpy(buffer, (void *) &(((uchar *) &(size))[position]), plength);
            position = 0;
            length -= plength;
            buffer += plength;

            if (length < 1) return plength;
        } else {
            position -= sizeof(size_t);
        }

        memcpy(buffer, (void *) &(((uchar *) &(value[0]))[position]), length);

        return length + plength;
    }

    static SharedBuffer wrap(const any_container<T> value) { return SharedBuffer(new ListBuffer<T>(value)); }

private:

    std::vector<T> value;
    
};

class OffsetBufferMessage : public Message {
public:

    OffsetBufferMessage(const SharedBuffer buffer, size_t offset = 0);

    virtual ~OffsetBufferMessage();

    virtual size_t get_length() const;

    virtual size_t copy_data(size_t position, uchar* buffer, size_t length) const;

private:

    SharedBuffer buffer;
    size_t offset;
    
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
    int16_t read_short();

    /**
     * @return next integer
     */
    int32_t read_integer();

    /**
     * @return next long integer
     */
    int64_t read_long();

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

    size_t get_position() const;

    size_t get_length() const;

    void copy_data(uchar* buffer, size_t length = 0);

    void debug_peek(size_t position, size_t length) const;

private:

    SharedMessage message;

    size_t position;

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
    size_t n = reader.read<size_t>();
    dst.resize(n);
    for (size_t i = 0; i < n; i++) 
        read(reader, dst[i]);
}

class MessageWriter {
    friend MemoryBuffer;
    friend MessageReader;
public:
    /**
     *
     */
    virtual ~MessageWriter();

    MessageWriter(size_t length = 0);

    MessageWriter(uchar* buffer, size_t length);

    template<typename T> void write(const T& value) {

        static_assert(std::is_arithmetic<T>::value, "Only primitive numeric types supported here");
        write_buffer((uchar *)&value, sizeof(T));

    }

    int write_short(int16_t value);

    int write_integer(int32_t value);

    int write_long(int64_t value);

    int write_bool(bool value);

    int write_char(char value);

    int write_float(float value);

    int write_double(double value);

    int write_string(const string& value);

    virtual int write_buffer(const uchar* buffer, size_t len);

    virtual int write_buffer(MessageReader& reader, size_t len);

    SharedMessage clone_data();

    size_t get_length();

protected:

    bool data_owned;
    uchar *data;
    size_t data_length;
    size_t data_position;

};

class DummyWriter: public MessageWriter {
public:

    DummyWriter();

    virtual ~DummyWriter();

    virtual int write_buffer(const uchar* buffer, size_t len);

    virtual int write_buffer(MessageReader& reader, size_t len);

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

template<typename T> size_t message_length(const T data) {
    
    DummyWriter writer;
    write(writer, data);

    return writer.get_length();

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

    int get_error() const;

    uint64_t get_read_data() const;

private:

    int fd;

    void reset();

    SharedMessage process_buffer();

    int error;

    int state;
    int header_value;

    int message_channel;
    uchar *data;
    size_t data_length;
    size_t data_current;
    uint64_t data_read_counter;
    uint64_t total_data_read;

    uchar *buffer;
    ssize_t buffer_length;
    ssize_t buffer_position;

};

#define MESSAGE_CALLBACK_SENT 0
#define MESSAGE_CALLBACK_DROPPED 1

typedef function<void(const SharedMessage, int state)> MessageCallback;

class StreamWriter {
public:

    StreamWriter(int fd, std::size_t size = MESSAGE_MAX_QUEUE);
    ~StreamWriter();

    bool add_message(const SharedMessage msg, int priority, MessageCallback callback = NULL);

    bool write_messages();

    int get_error() const;

    int get_queue_size() const;

    int get_queue_limit() const;

    unsigned long get_written_data() const;

    unsigned long get_dropped_data() const;

protected:

    class BoundedQueue;

    class MessageContainer {
    public:
        MessageContainer() : priority(0), time(0) {}
        MessageContainer(SharedMessage message, int priority, long time, MessageCallback callback = NULL) :
        message(message), priority(priority), time(time), callback(callback) {}

        SharedMessage message;
        int priority;
        long time;
        MessageCallback callback;

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

    BoundedQueue *outgoing;

    MessageContainer pending;

    uchar *buffer;
    size_t buffer_position;
    size_t buffer_length;

    int state;
    size_t message_position;
    size_t message_length;

    uint64_t time;

    uint64_t total_data_written;
    uint64_t total_data_dropped;

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

}

#endif
