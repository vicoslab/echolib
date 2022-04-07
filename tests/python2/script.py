from __future__ import print_function
from __future__ import absolute_import
from __future__ import division
from __future__ import unicode_literals

import sys

import echolib

from time import time

if __name__ == "__main__":

    channel_in  = sys.argv[1]
    channel_out = sys.argv[2]

    counter = 0

    def __callback(message):
        
        global counter
        counter += 1

        print("Msg %d: %s " % (counter, echolib.MessageReader(message).readString()))
        

    loop   = echolib.IOLoop()
    client = echolib.Client()
    loop.add_handler(client)


    subscriber = echolib.Subscriber(client, channel_in, u"string", __callback)
    publisher  = echolib.Publisher(client, channel_out, u"string") 

    t = time()

    while loop.wait(100):
    
        writer = echolib.MessageWriter()
        writer.writeString("Hello there")

        print("Send")

        publisher.send(writer)

        t = time()
