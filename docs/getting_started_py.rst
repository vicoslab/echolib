Getting started - Python
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


Writing a simple program
------------------------
Let's examine the included chat example to get an idea of how to write programs using Echo. A chat application is one of the simplest examples of interprocess communication.::
    
    import sys
    import time
    import pyecho
    import thread


    def message(reader):
        name = reader.readString()
        message = reader.readString()
        print name + ": " + message

    def write(communicator,pub,name):
        while communicator.isConnected():
            message = raw_input()
            writer = pyecho.MessageWriter()
            writer.writeString(name)
            writer.writeString(message)
            pub.send(writer)

    def main():
        if len(sys.argv) < 2:
            raise Exception('Missing socket')

        communicator = pyecho.Communicator(sys.argv[1])

        name = raw_input("Please enter your name:")   

        sub = pyecho.Subscriber(communicator, "chat", "string pair", message)
        pub = pyecho.Publisher(communicator, "chat", "string pair")

        thread.start_new_thread(write,(communicator,pub,name,))
        try:
            while communicator.wait(10):
                time.sleep(0.001)
        except KeyboardInterrupt:
            communicator.disconnect()

        sys.exit(1)

    if __name__ == '__main__':
        main() 

All of Echo's functions are defined in the pyecho library, so we first need to include it::
   
    import pyecho

If we want to use the library, we first need to establish a connection with the central daemon process. This is done by creating a communicator object to which we pass the socket path
that we specified when running the daemon. In this case, the socket path is provided to the program as an argument::

    communicator = pyecho.Communicator(sys.argv[1])

Next, we create a publisher and a subscriber. The publisher will send the messages that the user enters, while the subscriber will display received messages on screen::

    pub = pyecho.Publisher(communicator, "chat", "string pair")

When creating the publisher, we need to specify the communicator we defined earlier, a channel name (in our case chat) and the type of a message. Our messages will consist of two strings (one for the name of the user and one for the user's message), so the type is "string pair"::

    sub = pyecho.Subscriber(communicator, "chat", "string pair", message)

Creating a subscriber is almost the same, except we also need to provide a callback function. In this case, the function in called message and is defined on top of the source code::

    def message(reader):
        name = reader.readString()
        message = reader.readString()
        print name + ": " + message

Everytime we receive a message, this callback finction will be called on it. Extracting data from the message is as simple as calling appropriate read methods on the recieved message.
Since our chat messages consist of a pair of strings, we call readString() twice, then print the result.
Now we know how to handle received messages. Sending them works mostly the same::

    def write(communicator,pub,name):
        while communicator.isConnected():
            message = raw_input()
            writer = pyecho.MessageWriter()
            writer.writeString(name)
            writer.writeString(message)
            pub.send(writer)

the write function represents the main loop of our chat program. It reads in a text message, then sends the user's name and the read message to all chat programs that are currently running (or, more specifically, to the chat channel on the daemon, which then handles the distribution). We first create a message writer, write the strings with writeString(), and send the message through the publisher we defined earlier. Now we simply start this function on a new thread::

    thread.start_new_thread(write,(communicator,pub,name,))

The final thing left to do is define when we want to process the received messages. Since this is a simple chat program, we will simply continuously process them in the main thread::

    while communicator.wait(10):
        time.sleep(0.001)

Every time we call communicator.wait(), the communicator will collect all messages send to the subscribers that were defined with it and call their callback function. Since we don't wand to completelly overload the communicator, we add a small delay into the loop.
Once we are done, we need to safelly disconnect all publishers and subscribers from the daemon. We van do this using the disconnect() method of the communicator::

    communicator.disconnect()
