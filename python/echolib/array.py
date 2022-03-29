from __future__ import absolute_import
from __future__ import division
from __future__ import print_function
from __future__ import unicode_literals

from builtins import super
from future import standard_library
standard_library.install_aliases()
from echolib import _echo

class ArraySubscriber(_echo.Subscriber):

    def __init__(self, client, alias, callback):
        def _read(message):
            reader = _echo.MessageReader(message)
            return _echo.readArray(reader)

        super().__init__(client, alias, "array", lambda x: callback(_read(x)))

class ArrayPublisher(_echo.Publisher):

    def __init__(self, client, alias):
        super().__init__(client, alias, "array")

    def send(self, obj):
        writer = _echo.MessageWriter()
        _echo.writeArray(writer, obj)
        super().send(writer)

class TensorSubscriber(_echo.Subscriber):

    def __init__(self, client, alias, callback):
        def _read(message):
            reader = _echo.MessageReader(message)
            return _echo.readTensor(reader)

        super().__init__(client, alias, "tensor", lambda x: callback(_read(x)))

class TensorPublisher(_echo.Publisher):

    def __init__(self, client, alias):
        super().__init__(client, alias, "tensor")

    def send(self, obj):
        writer = _echo.MessageWriter()
        _echo.writeTensor(writer, obj)
        super().send(writer)