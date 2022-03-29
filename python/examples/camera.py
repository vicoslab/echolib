# -*- Mode: Python; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-
from __future__ import print_function
from __future__ import absolute_import
from __future__ import division
from __future__ import unicode_literals

from future import standard_library
standard_library.install_aliases()
from builtins import object
import sys
import echolib
from echolib.camera import FramePublisher, Frame
import traceback

try:
    import cv2
except ImportError:
    print("OpenCV not provided, camera acquistion not possible")
    sys.exit(-1)

import numpy as np

class DummyCamera(object):

    def read(self):
        return True, (np.random.rand(240, 320, 3)*255).astype(np.uint8)

def main():

    loop = echolib.IOLoop()
    client = echolib.Client()
    loop.add_handler(client)

    output = FramePublisher(client, "camera")
    #count = echolib.SubscriptionWatcher(client, "camera")

    camera = cv2.VideoCapture(0)

    if not camera.isOpened():  
        print("Camera not available")
        camera = DummyCamera()

    try:
        while loop.wait(100):
            _, image = camera.read()
            output.send(Frame(image=image))

    except Exception as e:
        traceback.print_exception(e)

if __name__ == '__main__':
    main()
