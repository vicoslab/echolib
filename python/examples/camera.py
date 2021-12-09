# -*- Mode: Python; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-

import sys
import echolib
from echolib.camera import FramePublisher, Frame

try:
    import cv2
except ImportError:
    print("OpenCV not provided, camera acquistion not possible")
    sys.exit(-1)

def main():

    loop = echolib.IOLoop()
    client = echolib.Client()
    loop.add_handler(client)

    output = FramePublisher(client, "camera")
    #count = echolib.SubscriptionWatcher(client, "camera")

    camera = cv2.VideoCapture(0)

    if not camera.isOpened():  
        print("Camera not available")
        sys.exit(-1)  

    try:
        while loop.wait(100):
            #if count.subscribers > 0:
            #    print("sending")
            _, image = camera.read()
            output.send(Frame(image=image))

    except KeyboardInterrupt:
        pass

    sys.exit(1)

if __name__ == '__main__':
    main()
