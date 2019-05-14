# -*- Mode: Python; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-

import sys
import time
import pyecho
import thread

def message(message):
    reader = pyecho.MessageReader(message)
    name = reader.readString()
    message = reader.readString()
    print name + ": " + message

def write(client,pub,name):
    while client.isConnected():
        message = raw_input()
        writer = pyecho.MessageWriter()
        writer.writeString(name)
        writer.writeString(message)
        pub.send(writer)

def main():
    if len(sys.argv) < 2:
        raise Exception('Missing socket')

    loop = pyecho.IOLoop()

    client = pyecho.Client()
    loop.add_handler(client)

    name = raw_input("Please enter your name:")

    sub = pyecho.Subscriber(client, "chat", "string pair", message)

    pub = pyecho.Publisher(client, "chat", "string pair")

    thread.start_new_thread(write,(client,pub,name,))
    try:
        while loop.wait(10):
            # We have to give the write thread some space
            time.sleep(0.001)
    except KeyboardInterrupt:
        pass

    sys.exit(1)

if __name__ == '__main__':
    main()
