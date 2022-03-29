# -*- Mode: Python; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*-
from __future__ import print_function
from __future__ import absolute_import
from __future__ import division
from __future__ import unicode_literals

from builtins import str
from future import standard_library
standard_library.install_aliases()
from builtins import object
import sys
import echolib
from echolib.camera import FrameSubscriber

try:
    import cv2

    def display(frame):
        canvas = cv2.cvtColor(frame.image, cv2.COLOR_RGB2BGR)
        cv2.putText(canvas, str(frame.header.timestamp), (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 1.0, (255, 0, 0), 3)
        cv2.imshow("Image", canvas)
        cv2.waitKey(2)

except ImportError:
    print("OpenCV not provided, image visualization not possible")
    display = lambda: None

def main():

    class FrameCollector(object):

        def __init__(self):
            self.frame = None

        def __call__(self, x):
            self.frame = x

    collector = FrameCollector()

    loop = echolib.IOLoop()
    client = echolib.Client()
    loop.add_handler(client)

    sub = FrameSubscriber(client, "camera", collector)

    try:
        while loop.wait(10):
            if collector.frame is not None:
                display(collector.frame)
                collector.frame = None

    except KeyboardInterrupt:
        pass

    sys.exit(1)

if __name__ == '__main__':
    main()
