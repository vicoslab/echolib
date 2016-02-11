.. highlight:: c++

Getting started - C++
================================

Basic concepts
--------------

* Echo is an interprocess communication library. It allows different processes or programs to share data between themselves.
* Echo uses a publish/subscribe system to allow for organized sharing of data between processes. All communication is organized through named channels. 
  A message isn't sent to specific processes, but to a channel, and is then distributed automatically to every process that is subscribed to that channel.
* Handling of received messages is done through callback functions. When a process subscribes to a channel, it also provides a callback function that will be 
  called on the received messages.
* Echo uses a central process (also called a dameon) to manage it's communication. This the deamon needs to run in the background if you want to use the library.
  To start the daemon, navigate to the build directory of the project and run ./echodaemon socket_path, where socket path is the path to a socket through which the processes
  will communicate. The programs you are writing will use this socket path to connect with the daemon.
* All Echo methods are thread safe.
* Echo nativly uses recent C++11 and C++14 features such as smart pointers and anonymous functions. This means that you don't need to use libraries like boost for this functionallity.


Writing a simple program
------------------------
Let's examine the included chat example to get an idea of how to write programs using Echo. A chat application is one of the simplest examples of interprocess communication.::

    #include "client.h"
    #include "datatypes.h"

    template<> inline shared_ptr<Message>
    echolib::Message::pack<pair<string, string> >
    (int channel, const pair<string, string> &data)
    {
        MessageWriter writer(data.first.size() + data.second.size() + 8);

        writer.write_string(data.first);
        writer.write_string(data.second);

        return make_shared<BufferedMessage>(channel, writer);
    }

    template<> inline shared_ptr<pair<string, string> > 
    echolib::Message::unpack<pair<string, string> >(SharedMessage message)
    {
        MessageReader reader(message);

        string user = reader.read_string();
        string text = reader.read_string();

        return make_shared<pair<string, string> >(user, text);
    }

    int main(int argc, char** argv)
    {

        SharedClient client = make_shared<echolib::Client>(string(argv[1]));

        string name;
        cout << "Enter your name\n";
        std::getline(std::cin, name);

        std::mutex mutex;

        function<void(shared_ptr<pair<string, string> >)> chat_callback = 
        [&](shared_ptr<pair<string, string> > m){
            const string name = m->first;
            const string message = m->second;
            std::cout<< name << ": " << message << std::endl;
        };

        function<void(int)> subscribe_callback = [&](int m){
            std::cout << "Total subscribers: " << m << std::endl;
        };

        TypedSubscriber<pair<string, string> > sub(client, "chat", chat_callback);
        SubscriptionWatcher watch(client, "chat", subscribe_callback);
        TypedPublisher<pair<string, string> > pub(client, "chat");

        std::thread write([&](){
            while (client->is_connected()){
                string message; 
                std::getline(std::cin, message);
                pub.send(pair<string, string>(name, message));
            }
            
        });

        while(client->wait(10)){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        exit(0);
    }

Echo's functionallity is defined in several header files, so we need to include them first::

    #include "client.h"
    #include "datatypes.h"

First, we need to define the datatype we will be sending. In this example, we will be sending a pair of two strings - a pair<string,string>. If we want to transfer messages between two processes, we first need to tell echo how to convert them into an appropriate binary form. To do this, we need to define two functions, one to encode the object into a binary representation and one to decode the received object from it's binary representation. This is done through the first two function::

    template<> inline shared_ptr<Message>
    echolib::Message::pack<pair<string, string> >(int channel, const pair<string, string> &data)
    {
        MessageWriter writer(data.first.size() + data.second.size() + 8);

        writer.write_string(data.first);
        writer.write_string(data.second);

        return make_shared<BufferedMessage>(channel, writer);
    }

The first function is pack, which transforms the c++ object into the appropriate binary representation. It ovverrides the base pack method of the message class. The important parts of the function signature is the template argument of pack, which represents the datatype we will be sending, and the second argument, which is the concrete data we will be sending. Everything else will remain the same no matter what datatype we are sending.

Encoding the data is done through a helper class called MessageWriter. First, we instantiate the writer, passing the total size of the data into the constructor. Then we use writer's write methods to encode the data. Since we are sending two string we use write_string twice, but the MessageWriter class also supports other base types. Once wi have written all the data, we return it as a shared pointer to the message.::

    template<> inline shared_ptr<pair<string, string> >
    echolib::Message::unpack<pair<string, string> >(SharedMessage message)
    {
        MessageReader reader(message);

        string user = reader.read_string();
        string text = reader.read_string();

        return make_shared<pair<string, string> >(user, text);
    }

The second function: unpack, takes care of the reverse operation - transforming the received binary data into a c++ object. It's similar to the pack function, onlky it uses the helper class MessageReader to read the binary data.


If we want to use the library, we first need to establish a connection with the central daemon process. This is done by creating a SharedClient object to which we pass the socket path
that we specified when running the daemon. In this case, the socket path is provided to the program as an argument::

    SharedClient client = make_shared<echolib::Client>(string(argv[1]));

Here, SharedClient is a C++11 shared pointer, so it needs to be created with make_shared()
Next, we create a publisher and a subscriber. The publisher will send the messages that the user enters, while the subscriber will display received messages on screen::

    TypedPublisher<pair<string, string> > pub(client, "chat");

When creating the publisher, we need to specify the client we defined earlier, a channel name (in our case chat) and the type of a message, which is provided as a template argument. Our messages will consist of two strings (one for the name of the user and one for the user's message), so the type is pair<string, string>. We need to define the pack and unapck methods for every class we want to use, as we did above::

    TypedSubscriber<pair<string, string> > sub(client, "chat", chat_callback);

Creating a subscriber is almost the same, except we also need to provide a callback function. In this case, the function in called chat_callback and is defined on top of the source code::

    function<void(shared_ptr<pair<string, string> >)> chat_callback = 
    [&](shared_ptr<pair<string, string> > m){
            const string name = m->first;
            const string message = m->second;
            std::cout<< name << ": " << message << std::endl;
        };

This is a C++11 lambda function that takes one argument (which is  a pointer ro the same as the type of the message, ie. shared_ptr<pair<string, string>>) and returns void.
The message we receive will be of type shared_ptr<pair<string, string>>, so we can call methods directly on the received message m to extract the first and second component and print them on the screen. This function will be called everytime we receive a message.
Now we know how to handle received messages, but we also need to actually send our messages::
    
    std::thread write([&](){
            while (client->is_connected()){
                string message; 
                std::getline(std::cin, message);
                pub.send(pair<string, string>(name, message));
            }
        });
   
We will handle message sending on it's own thread. Sending a message is as simple as calling the send() method of the defined publisher, which will take one argument of the type we defined when creating the publisher (so pair<string, string>). As such, the only thing we need to do to send the message is:: 
    
    pub.send(pair<string, string>(name, message));


The final thing left to do is define when we want to process the received messages. Since this is a simple chat program, we will simply continuously process them in the main thread::

    while(client->wait(10)){
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

Every time we call client->wait(), the communicator will collect all messages send to the subscribers that were defined with it and call their callback function. Since we don't wand to completelly overload the communicator, we add a small delay into the loop.
We don't have to worry about manually disconnecting the publisher and subscriber from the daemon, since this will be handled automatically by their destructors.
