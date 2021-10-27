
import unittest

from echolib import Client, IOLoop

class Tests(unittest.TestCase):

    def test_tensor(self):
        from echolib.array import TensorPublisher, TensorSubscriber

        client = Client()