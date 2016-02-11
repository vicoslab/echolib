For developers
================================


Project structure
-----------------

Daemon
######
The daemon is the central process that runs in the background, implemented in the daemon.cpp file. The main event loop first reads all pending epoll events with the epoll_wait() call.
Then, it iterates over the events and performs one of the three actions based on the type of the event:
    
    * If it's an error event, it prints an error report and terminates the daemon
    * If it's an event on the daemon's listening socket, this indicates that it's an incoming connection from a new client. In this case, the daemon accepts the connection and registers the socket with epoll so that it can receive events from it
    * If it's an event on a socket belonging to a client, the daemon notifies the router class that a message has to be parsed.

Routing
#######
The the classes in the routing.h file are used by the daemon to properly receive and transmit messages. The header file defines three classses. The Channel class is the internal representation of a channel with methods to add or remove subscribers and watchers and send messages to all channel subscribers. The Client class is used by the daemon to represent connected processes. The router class is used to keep track of all the clients and the channels they are connected to, as well as to handle the automatic transmission of received messages to their proper recipients.

Message
#######
The message.h header defines various message types and classes used for encoding and decoding of messages.

Client
######
The client.h header defines classes that are used by echo's users to connect to the daemon. It defines the basic client, subscriber and watcher classes that are used to connect to and communicate with the daemon. It also defines the advanced ChunkedSubscriber variant of the subscriber class that allows for the automatic splitting of messages.

Datatypes
#########
The datatypes.h header provides typed variants of the publisher and subscriber classes.

Python
######
Python.cpp uses pybind11 to define python bindings of the library

Debug
#####
The debug.h header defines helper methods used for logging and debugging of applications that use echo.
