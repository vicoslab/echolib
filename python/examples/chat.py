# -*- Mode: Python; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-

import sys
import time
import echolib
import threading

def main():

    def message(message):
        reader = echolib.MessageReader(message)
        name = reader.readString()
        message = reader.readString()
        print(name + ": " + message)

    def write(client, pub, name):
        while client.isConnected():
            try:
                message = input()
                writer = echolib.MessageWriter()
                writer.writeString(name)
                writer.writeString(message)
                pub.send(writer)
            except EOFError:
                break

    loop = echolib.IOLoop()

    client = echolib.Client()
    loop.add_handler(client)

    name = input("Please enter your name:")

    sub = echolib.Subscriber(client, "chat", "string pair", message)

    pub = echolib.Publisher(client, "chat", "string pair")

    thread = threading.Thread(target=write, args=(client,pub,name,))
    thread.start()

    try:
        while loop.wait(10) and thread.is_alive():
            # We have to give the write thread some space
            time.sleep(0.001)
    except KeyboardInterrupt:
        pass

    sys.exit(1)

if __name__ == '__main__':
    main()
