/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <err.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <algorithm>
#include <malloc.h>
#include <cmath>

#include "debug.h"
#include <echolib/message.h>
#include "algorithms.h"

using namespace std;

namespace echolib
{

#define MESSAGE_DELIMITER ((char)0x0F)

    class StreamWriter::BoundedQueue : public bounded_priority_queue<MessageContainer, vector<MessageContainer>,
                                                                     function<bool(MessageContainer, MessageContainer)>>
    {
    public:
        BoundedQueue(std::size_t size, const function<bool(MessageContainer, MessageContainer)> &comp) : bounded_priority_queue(size, comp) {}
        ~BoundedQueue(){};

        using bounded_priority_queue::max_size;
        using bounded_priority_queue::pop_top;
        using bounded_priority_queue::push;
        using bounded_priority_queue::size;
    };

    const char *EndOfBufferException::what() const throw()
    {
        return "End of buffer";
    }

    void Buffer::inspect_data(ostream& output) const
    {
        uchar* temp = new uchar[get_length()];
        copy_data(0, temp, get_length());
        print_buffer(output, temp, get_length());
        delete[] temp;
    }

    MessageReader::~MessageReader()
    {
    }

    MessageReader::MessageReader(SharedMessage message) : message(message), position(0)
    {
    }

    int16_t MessageReader::read_short()
    {

        int16_t result = 0;

        copy_data((uchar *)&result, (size_t)sizeof(int16_t));

        return result;
    }

    int32_t MessageReader::read_integer()
    {

        int32_t result = 0;

        copy_data((uchar *)&result, (size_t)sizeof(int32_t));

        return result;
    }

    bool MessageReader::read_bool()
    {

        char result = 0;

        copy_data((uchar *)&result, (size_t)sizeof(char));

        return result > 0;
    }

    int64_t MessageReader::read_long()
    {

        int64_t result = 0;

        copy_data((uchar *)&result, (size_t)sizeof(int64_t));

        return result;
    }

    char MessageReader::read_char()
    {

        char result = 0;

        copy_data((uchar *)&result, (size_t)sizeof(char));

        return result;
    }

    float MessageReader::read_float()
    {

        float result = 0;

        copy_data((uchar *)&result, (size_t)sizeof(float));

        return result;
    }

    double MessageReader::read_double()
    {

        double result = 0;

        copy_data((uchar *)&result, (size_t)sizeof(double));

        return result;
    }

    std::string MessageReader::read_string()
    {
        size_t len = (size_t)read_integer();

        std::string result;

        if (len)
        {
            result.resize(len);
            copy_data((uchar *)&result[0], (size_t)sizeof(char) * len);
        }

        return result;
    }

    void MessageReader::debug_peek(size_t position, size_t length) const
    {

        length = min(message->get_length() - position, length);

        if (length < 1)
            return;

        uchar *temp = new uchar[length];

        message->copy_data(position, temp, length);

        for (size_t i = 0; i < length; i++)
        {
            std::cout << format_string(" %04d: (%#04X) %*d %c", (int)(i + position), temp[i], 4, temp[i], (int)temp[i]) << std::endl;
        }

        delete[] temp;
    }

    size_t MessageReader::get_position() const
    {
        return position;
    }

    size_t MessageReader::get_length() const
    {
        return message->get_length();
    }

    void MessageReader::copy_data(uchar *buffer, size_t length)
    {

        if (length < 1)
            length = message->get_length() - position;

        if (message->get_length() - position < length)
        {
            throw EndOfBufferException();
        }

        message->copy_data(position, buffer, length);

        position += length;
    }

    MessageWriter::~MessageWriter()
    {

        if (data_owned && data)
            free(data);
    }

    MessageWriter::MessageWriter(size_t length) : data_owned(true), data_length(length), data_position(0)
    {
        if (data_length > 0)
        {
            data = (uchar *)malloc(sizeof(uchar) * data_length);
        }
        else
        {
            data = NULL;
        }
    }

    MessageWriter::MessageWriter(uchar *buffer, size_t length) : data_owned(false), data(buffer), data_length(length), data_position(0)
    {
    }

    int MessageWriter::write_buffer(const uchar *buffer, size_t len)
    {

        if (len > data_length - data_position)
        {
            if (!data_owned)
                throw EndOfBufferException();
            if (!data_length)
            {
                data_length = len;
                data = (uchar *)malloc(sizeof(uchar) * data_length);
            }
            else
            {
                data_length = data_position + len;
                data = (uchar *)realloc(data, sizeof(uchar) * data_length);
            }
        }

        memcpy(&(data[data_position]), buffer, len);

        data_position += len;

        return data_length;
    }

    int MessageWriter::write_buffer(MessageReader &reader, size_t len)
    {
        len = min(len, reader.get_length() - reader.get_position());

        if (len > data_length - data_position)
        {
            if (!data_owned)
                throw EndOfBufferException();
            if (!data_length)
            {
                data_length = len;
                data = (uchar *)malloc(sizeof(uchar) * data_length);
            }
            else
            {
                data_length = data_position + len;
                data = (uchar *)realloc(data, sizeof(uchar) * data_length);
            }
        }

        reader.copy_data(&(data[data_position]), len);

        data_position += len;

        return len;
    }

    int MessageWriter::write_short(int16_t value)
    {

        return write_buffer((uchar *)&value, sizeof(int16_t));
    }

    int MessageWriter::write_integer(int32_t value)
    {

        return write_buffer((uchar *)&value, sizeof(int32_t));
    }

    int MessageWriter::write_bool(bool value)
    {
        int v = value ? 1 : 0;
        return write_buffer((uchar *)&v, sizeof(char));
    }

    int MessageWriter::write_long(int64_t value)
    {

        return write_buffer((uchar *)&value, sizeof(int64_t));
    }

    int MessageWriter::write_char(char value)
    {

        return write_buffer((uchar *)&value, sizeof(char));
    }

    int MessageWriter::write_float(float value)
    {

        return write_buffer((uchar *)&value, sizeof(float));
    }

    int MessageWriter::write_double(double value)
    {

        return write_buffer((uchar *)&value, sizeof(double));
    }

    int MessageWriter::write_string(const std::string &value)
    {

        write_integer(value.size());

        return write_buffer((const uchar *)value.c_str(), value.size()) + sizeof(int);
    }

    size_t MessageWriter::get_length()
    {

        return data_position;
    }

    DummyWriter::DummyWriter() : MessageWriter(NULL, false) {}

    DummyWriter::~DummyWriter() {}

    int DummyWriter::write_buffer(const uchar *buffer, size_t len)
    {

        data_position += len;

        return len;
    }

    int DummyWriter::write_buffer(MessageReader &reader, size_t len)
    {

        data_position += len;

        return len;
    }

    SharedMessage MessageWriter::clone_data()
    {

        shared_ptr<BufferedMessage> msg = make_shared<BufferedMessage>(get_length());

        memcpy(msg->get_buffer(), data, get_length());

        return msg;
    }

    StreamReader::StreamReader(int fd) : fd(fd)
    {
        data = NULL;
        buffer_length = 0;
        total_data_read = 0;
        data_read_counter = 0;
        buffer_position = 0;

        buffer = (uchar *)malloc(sizeof(char) * BUFFER_SIZE);
        reset();
    }

    StreamReader::~StreamReader()
    {

        reset();
        free(buffer);
    }

    shared_ptr<Message> StreamReader::read_message()
    {
        /* We have data on the fd waiting to be read. Read and
           display it. We must read whatever data is available
           completely, as we are running in edge-triggered mode
           and won't get a notification again for the same
           data. */
        error = 0;
        while (1)
        {
            /* Buffer position is increased in process_message() */
            if (buffer_position >= buffer_length)
            {
                buffer_length = ::read(fd, buffer, BUFFER_SIZE);
                buffer_position = 0;

                if (buffer_length == -1)
                {
                    /* If errno == EAGAIN, that means we have read all
                    data. So go back to the main loop. */
                    if (errno != EAGAIN)
                    {
                        error = -1;
                    }
                    break;
                }
                else if (buffer_length == 0)
                {
                    /* End of file. The remote has closed the
                      connection. */
                    error = -2;
                    break;
                }
            }

            shared_ptr<Message> msg = process_buffer();
            if (msg)
                return msg;
        }

        return NULL;
    }

    int StreamReader::get_error() const
    {
        return error;
    }

    uint64_t StreamReader::get_read_data() const
    {
        return total_data_read;
    }

    void StreamReader::reset()
    {
        state = 0;
        data_read_counter = 0;
        data_length = 0;
        data_current = 0;
        error = 0;

        if (data)
            free(data);
    }

    shared_ptr<Message> StreamReader::process_buffer()
    {
        bool complete = false;
        int i;
        for (i = buffer_position; i < buffer_length; i++)
        {
            switch (state)
            {
            case 0:
            {
                if (buffer[i] == MESSAGE_DELIMITER)
                {
                    state = 1;
                }
                else
                {
                    // TODO: What to do on error? Disconnect.
                    error = -5; // Illegal delimiter
                }

                break;
            }
            case 1:
            case 2:
            case 3:
            case 4:
            {
                data_length = data_length << 8;
                data_length |= (int)buffer[i];
                state++;
                break;
            }
            case 5:
            {
                if (data_length > MESSAGE_MAX_SIZE)
                {
                    error = -1;
                    break;
                }

                // TODO: test if total length too high
                // TODO: custom allocator?
                data = (uchar *)malloc(data_length * sizeof(char));

            } // intentional fallthrough
            case 6:
            {

                data_read_counter += buffer_length - i;
                if (data_read_counter >= data_length)
                {
                    memcpy(&(data[data_current]), &(buffer[i]), data_length - data_current);
                    i += data_length - data_current;
                    complete = true;
                }
                else
                {
                    memcpy(&(data[data_current]), &(buffer[i]), buffer_length - i - 1);
                    data_current += buffer_length - i;
                    state = 6; // Wait for more data
                    i = buffer_length;
                }

                break;
            }
            }

            if (error)
            {
                reset();
                return shared_ptr<Message>();
            }

            if (complete)
            {
                shared_ptr<Message> ptr(make_shared<BufferedMessage>(data, data_length, true));
                total_data_read += data_length;
                data = NULL;
                buffer_position = i;
                reset();
                return ptr;
            }
        }

        buffer_position = i;
        return shared_ptr<Message>();
    }

    bool StreamWriter::comparator(const MessageContainer &lhs, const MessageContainer &rhs)
    {
        return (lhs.priority < rhs.priority) || (lhs.priority == rhs.priority && lhs.time < rhs.time);
    }

    StreamWriter::StreamWriter(int fd, size_t size) : fd(fd), outgoing(new BoundedQueue(size, &StreamWriter::comparator)), time(0)
    {

        buffer = (uchar *)malloc(BUFFER_SIZE);
        message_position = 0;
        error = 0;
        total_data_written = 0;
        total_data_dropped = 0;
    }

    StreamWriter::~StreamWriter()
    {

        delete outgoing;

        if (buffer)
            free(buffer);
    }

    bool StreamWriter::add_message(SharedMessage msg, int priority, MessageCallback callback)
    {

        if (outgoing->empty() && !pending.message)
        {
            pending = MessageContainer(msg, priority, time++, callback);
            reset();
            write_messages();
            return true;
        }
        else
        {

            MessageContainer a(msg, priority, time++, callback);

            if (!outgoing->push(a))
            {

                MessageContainer rm = outgoing->push_over(a);
                total_data_dropped += rm.message->get_length();

                if (rm.callback)
                    rm.callback(rm.message, MESSAGE_CALLBACK_DROPPED);

                return false;
            }

            return true;
        }
    }

    bool StreamWriter::write_messages()
    {
        // started writing messages
        if (outgoing->empty() && !pending.message)
        {
            return true;
        }

        if (pending.is_empty())
        {
            pending = outgoing->top();
            outgoing->pop_top();
            reset();
        }

        while (!pending.is_empty())
        {
            if (process_message())
            {

                if (pending.callback)
                {

                    pending.callback(pending.message, MESSAGE_CALLBACK_SENT);
                }

                if (!outgoing->empty())
                {
                    pending = outgoing->top();
                    outgoing->pop_top();
                    reset();
                }
                else
                {
                    pending.reset();
                }
            }
            else
            {

                return false;
            }
        }
        return true;
    }

    int StreamWriter::get_error() const
    {

        return error;
    }

    unsigned long StreamWriter::get_written_data() const
    {
        return total_data_written;
    }

    unsigned long StreamWriter::get_dropped_data() const
    {
        return total_data_dropped;
    }

#define INTEGER_TO_ARRAY(A, I)   \
    {                            \
        A[0] = (I >> 24) & 0xFF; \
        A[1] = (I >> 16) & 0xFF; \
        A[2] = (I >> 8) & 0xFF;  \
        A[3] = I & 0xFF;         \
    }

    void StreamWriter::reset()
    {
        if (pending.is_empty())
            return;

        // Resets internal structure with new pending message

        message_length = pending.message->get_length();
        message_position = 0;

        int i = 0;

        buffer[i++] = MESSAGE_DELIMITER;

        INTEGER_TO_ARRAY((&buffer[i]), pending.message->get_length());

        i += sizeof(int32_t);

        buffer_position = 0;
        buffer_length = sizeof(uchar) + sizeof(int);
    }

    bool StreamWriter::process_message()
    {
        while (true)
        {

            ssize_t count = 0;

            if (buffer_position < buffer_length)
            {
                errno = 0;
                count = send(fd, &(buffer[buffer_position]), buffer_length - buffer_position, MSG_NOSIGNAL);

                if (count == -1)
                {

                    if (errno == EAGAIN || errno == EWOULDBLOCK)
                    {
                        return false;
                    }
                    else
                    {
                        error = -1;
                    }

                    return false;
                }
                else if (count == 0)
                {
                    /* End of file. The remote has closed the
                      connection. */
                    error = -2;
                    return false;
                }
                else
                {
                    buffer_position += count;
                    total_data_written += count;
                    continue;
                }
            }

            if (message_position >= message_length)
                break;

            count = pending.message->copy_data(message_position, buffer, min(BUFFER_SIZE, (int)(message_length - message_position)));

            message_position += count;
            buffer_position = 0;
            buffer_length = count;
        }
        return true;
    }

    int StreamWriter::get_queue_size() const
    {
        return outgoing->size();
    }

    int StreamWriter::get_queue_limit() const
    {
        return outgoing->max_size();
    }

    Message::Message()
    {
    }

    Message::~Message()
    {
    }

    BufferedMessage::BufferedMessage(uchar *data, size_t length, bool owned) : MemoryBuffer(data, length, owned), Message()
    {
    }

    BufferedMessage::BufferedMessage(int length) : MemoryBuffer(length), Message()
    {
    }

    BufferedMessage::BufferedMessage(MessageWriter &writer) : MemoryBuffer(writer), Message()
    {
    }

    BufferedMessage::~BufferedMessage()
    {
    }

    MemoryBuffer::MemoryBuffer(uchar *data, size_t length, bool owned) : data(data), data_length(length), data_owned(owned)
    {
    }

    MemoryBuffer::MemoryBuffer(size_t length) : data_length(length), data_owned(true)
    {
        data = (uchar *)malloc(sizeof(uchar) * length);
    }

    MemoryBuffer::MemoryBuffer(MessageWriter &writer)
    {
        data = writer.data;
        data_length = writer.data_position;

        writer.data_owned = false;
        data_owned = true;
    }

    MemoryBuffer::~MemoryBuffer()
    {
        if (data && data_owned)
        {
            free(data);
        }
    }

    size_t MemoryBuffer::get_length() const
    {
        return data_length;
    }

    size_t MemoryBuffer::copy_data(size_t position, uchar *dst, size_t length) const
    {
        length = min(length, data_length - position);

        if (length < 1)
            return 0;

        memcpy(dst, &data[position], length);

        return length;
    }

    uchar *MemoryBuffer::get_buffer() const
    {
        return data;
    }

    MultiBufferMessage::MultiBufferMessage() : Message(), length(0) {}

    MultiBufferMessage::MultiBufferMessage(any_container<SharedBuffer> buffers) : buffers(*buffers) { rebuild(); }

    MultiBufferMessage::~MultiBufferMessage()
    {
    }

    void MultiBufferMessage::rebuild()
    {

        offsets = vector<size_t>();
        length = 0;

        for (vector<SharedBuffer>::iterator it = buffers.begin(); it != buffers.end();)
        {
            if ((*it)->get_length() < 1)
            {
                buffers.erase(it);
                continue;
            }
            offsets.push_back(length);
            length += (*it)->get_length();
            it++;
        }
    }

    size_t MultiBufferMessage::get_length() const
    {

        return length;
    }

    size_t MultiBufferMessage::copy_data(size_t position, uchar *buffer, size_t length) const
    {

        vector<size_t>::const_iterator it = std::upper_bound(offsets.begin(), offsets.end(), position); // Find first element that is greater than position
        int index = (it - offsets.begin()) - 1;                                                         // Find index of previous element

        size_t offset = 0;
        for (unsigned int i = index; i < offsets.size(); i++)
        {
            size_t pos = position + offset - offsets[i];
            size_t len = min(buffers[i]->get_length() - pos, length - offset);
            if (len < 1)
                break; // We are done, no more data to copy
            offset += buffers[i]->copy_data(pos, &(buffer[offset]), len);
        }

        return offset;
    }

    OffsetBufferMessage::OffsetBufferMessage(const SharedBuffer buffer, size_t offset) : buffer(buffer), offset(offset)
    {
        if (offset > buffer->get_length())
            throw EndOfBufferException();
    }

    OffsetBufferMessage::~OffsetBufferMessage()
    {
    }

    size_t OffsetBufferMessage::get_length() const
    {

        return buffer->get_length() - offset;
    }

    size_t OffsetBufferMessage::copy_data(size_t position, uchar *buffer, size_t length) const
    {

        return this->buffer->copy_data(position + offset, buffer, length);
    }

    static inline bool is_invalid_atribute_char(char c)
    {
        return !(isalnum(c) || c == '.' || c == '_');
    }

    Dictionary::Dictionary()
    {
    }

    Dictionary::~Dictionary()
    {
    }

    bool Dictionary::valid_argument_name(const string &key)
    {
        return (find_if(key.begin(), key.end(), is_invalid_atribute_char) == key.end());
    }

    bool Dictionary::contains(const string key) const
    {

        std::map<std::string, std::string>::const_iterator it = arguments.find(key);

        return it != arguments.end();
    }

    DictionaryIterator Dictionary::begin() const
    {
        return arguments.begin();
    }

    DictionaryIterator Dictionary::end() const
    {
        return arguments.end();
    }

    size_t Dictionary::size() const
    {
        return arguments.size();
    }

}
