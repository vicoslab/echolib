.. highlight:: c++

Getting started - Advanced concepts
===================================

Watchers
--------

Sometimes, you want your programs to listen to changes on a particular communication channel, for example when somebody subscribes or unsubscribes for a channel. This can be useful when you want to, for example, only start sending data once you know somebody is listening to the channel. This can be accomplished through the concept of wacthers.
Watchers work similarly to ordinary subscribers, except that they do not actually read messages sent to a channel, but simply watch for changes on the channel. Interaction with changes on the channel is still accomplished with callback methods, similarly to the way they are used in ordinary subscribers.

Currently, this feature is still a work in progress, and only one type of a watcher is implemented directly: the SubscriptionWatcher, which wacthes for new subscribers on a given channel. Everytime a new subscriber subscribes, it fires a callback with the number of current subscribers on a channel. In c++, it can be used similarly to ordinary subscribers like this::

    function<void(int)> subscribe_callback = [&](int m){
        std::cout << "Total subscribers: " << m << std::endl;
    };

    SubscriptionWatcher watch(client, channel_name, subscribe_callback);


Custom Watchers
###############

If you want to define more complex watchers, you can define a subclass of the core Watcher class and override the on_event method. For an example, let's look at how the SubscriptionWatcher is implemented::

    class SubscriptionWatcher : public Watcher {
        public:
            SubscriptionWatcher(SharedClient client, const string &alias, 
                                function<void(int)> callback);
            virtual ~SubscriptionWatcher() {};
            virtual void on_event(SharedDictionary message);

        private:
            function<void(int)> callback;
        };

    SubscriptionWatcher::SubscriptionWatcher(SharedClient client, const string &alias,
                                             function<void(int)> callback):
    Watcher(client, alias), callback(callback) {
    }
    
    void SubscriptionWatcher::on_event(SharedDictionary message) {
        string type = message->get<string>("type", "");
        if (type == "subscribe" || type == "unsubscribe" || type == "summary") {
            int subscribers = message->get<int>("subscribers", 0);
            callback(subscribers);
        }
    }

The on_event function takes one argumet, a pointer to a Dictionary which contaions information about something that has happened on a channel. Currentyl, this dictionary contains two keys: type (the type of the event) and subscribers (current number of subscribers) where type can be one of:
    * Summary: misc. event. Currently fires when another watcher is added to the channel
    * Subscribe: a subscriber has subscribed to the channel
    * Unsubscribe: a subscriber has unsubscribed from the channel


Chunked messages
----------------
Splitting large messages into several smaller chunks can improve the performance tranferring data. To make the process of splitting the data easier you can enable Chunked messaging in the publisher and subscriber classes. To do this, simply pass true as the second template argument when creating a publisher or subscriber, like this::

    TypedPublisher <data_type, true> pub(client, channel_name);
    TypedSubscriber<data_type, true> sub(client, channel_name, callback);

This will automatically split sent messages into smaller chunks and merge them on the receiving end, which can improve performance, particularly on large messages.


Extending subscribers and publishers
------------------------------------
Instead of defining types and using TypedPublisher and TypedSubscriber you can directly extend the Publisher and Subscriber or their chunked variants classes for more control. Let's examine the OpenCV example to see how we can accomplish this::

    class ImageSubscriber : public ChunkedSubscriber {
    public:
        ImageSubscriber(SharedClient client, const string &alias, function<void(Mat&)> callback) :
        ChunkedSubscriber(client, alias, string("opencv matrix")), callback(callback) {
        }

        virtual ~ImageSubscriber() {
        }
        
        virtual void on_message(SharedMessage message) {
            try {
                MessageReader reader(message);
                int type = reader.read_integer();
                int cols = reader.read_integer();
                int rows = reader.read_integer();

                Mat data(rows, cols, type);

                //copy data simply copies binary data from it's current position of
                //length in the second argument into the first argument
                reader.copy_data(data.data, data.cols * data.rows * data.elemSize());

                callback(data);
            } catch (echolib::ParseException &e) {
                Subscriber::on_error(e);
            }
        };

    private:
        function<void(Mat&)> callback;
    };

The Subscriber is the one that receives messages, so you will need to override the on_message method to properly transform the received data before passing to the callback. Just like in the chat example, you can use a MessageReader to parse data then pass this data to the callback. You will also need to set the type of the variable the callback will be operating on.::

    class ImagePublisher : public ChunkedPublisher {
    public:
        ImagePublisher(SharedClient client, const string &alias) : ChunkedPublisher(client, alias, string("opencv matrix")) {}

        virtual ~ImagePublisher() {}
        
        bool send(Mat &mat) {
            shared_ptr<MemoryBuffer> header = make_shared<MemoryBuffer>(3 * sizeof(int));
            MessageWriter writer(header->get_buffer(), header->get_length());
            writer.write_integer(mat.type());
            writer.write_integer(mat.cols);
            writer.write_integer(mat.rows);

            vector<SharedBuffer> buffers;
            buffers.push_back(header);
            buffers.push_back(make_shared<MatBuffer>(mat));
            shared_ptr<Message> message = make_shared<MultiBufferMessage>(get_channel_id(), buffers);

            return send_message_internal(message);
        }

    private:

        class MatBuffer : public Buffer {

            public:
            MatBuffer(Mat& mat) : mat(mat) {
                mat_length =  mat.cols * mat.rows * mat.elemSize();
            }
            virtual ~MatBuffer() {};

            virtual ssize_t get_length() const
            {
                return mat_length;
            }

            virtual ssize_t copy_data(ssize_t position, uchar* buffer, ssize_t length) const
            {
                length = min(length, mat_length - position);
                if (length < 1) return 0;

                memcpy(buffer, &(mat.data[position]), length);
                return length;
            }

            private:
            Mat mat;
            ssize_t mat_length;
        };
    };

The publisher will be the class that sends your message, so it needs to implement methods to transform your object oriented data into a binary representation. This binary representation consists of one or more binary buffers represented by the Buffer class. Looking at the custom buffer implementation of MatBuffer, we can see that a Buffer class needs to implement two methods: get_length (which returns the size in bytes of the message) and copy_data(), which copies the binary representation of the data to the provided buffer argument. Since this message can be split into multiple chunks (for the above chunked messaging), it needs to be able to convert arbitrarily sized portions of our object into a binary representations. This portions are represented by the position (which indicates the start of the chunk) and length (which indicates the length of the chunk) arguments. The copy_data() function needs to copy the chunk of data into the provided buffer argument and returns the number of bytes written.

The actuall subscriber only needs to override the send() method, which takes in the object that we want to send and transforms it into a binary message. In this case, the message is split into two buffers that represent two parts of the message: the header and the body. The header uses MessageWriter to encode the data, similarly to how it was used earlier, while the data buffer uses the custom MatBuffer class described above to encode the actual data. After both buffers are written, they are simply encoded into the message. Since we are using multiple buffers, we can use the MultiBufferMessage class to easily represent out message. After the message is created, we need to send it with the send_message_internal() function.