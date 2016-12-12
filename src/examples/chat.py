# -*- Mode: Python; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- 

import sys
import time
import echolib
import thread

def message(message):
    reader = echolib.MessageReader(message)
    name = reader.readString()
    message = reader.readString()
    print name + ": " + message

def write(client,pub,name):
    while client.isConnected():
        message = raw_input()
        writer = echolib.MessageWriter()
        writer.writeString(name)
        writer.writeString(message)
        pub.send(writer)

def main():
    if len(sys.argv) < 2:
        raise Exception('Missing socket')

    loop = echolib.IOLoop()

    client = echolib.Client(loop, sys.argv[1])

    name = raw_input("Please enter your name:")   

    sub = echolib.Subscriber(client, "chat", "string pair", message)

    pub = echolib.Publisher(client, "chat", "string pair")

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
