# -*- Mode: Python; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- 

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
    #watcher = pyecho.Watcher(communicator, "chat")
    pub = pyecho.Publisher(communicator, "chat", "string pair")

    thread.start_new_thread(write,(communicator,pub,name,))
    try:
        while communicator.wait(10):
            # We have to give the write thread some space
            time.sleep(0.001)
    except KeyboardInterrupt:
        communicator.disconnect()

    sys.exit(1)

if __name__ == '__main__':
    main() 
