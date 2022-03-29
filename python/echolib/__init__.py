from __future__ import print_function
from __future__ import absolute_import
from __future__ import division
from __future__ import unicode_literals

from builtins import int
from builtins import super
from builtins import range
from builtins import str
from future import standard_library
standard_library.install_aliases()
try:
    import echolib.pyecho as _echo
except ImportError as ie:
    raise ImportError("Echo binary library not found", ie)

IOLoop = _echo.IOLoop
IOBase = _echo.IOBase
Client = _echo.Client
Publisher = _echo.Publisher
Subscriber = _echo.Subscriber
MessageReader = _echo.MessageReader
MessageWriter = _echo.MessageWriter

double = type('double', (), {})
char = type('char', (), {})
long = type('long', (), {})

_type_registry = {}

def router(address="/tmp/echo.sock", verbose=False):
    _echo.router(address, verbose)

def registerType(cls, read, write, convert=None):
    if cls.__name__ in _type_registry:
        raise Exception("Type %s already registered" % cls.__name__)
    _type_registry[cls.__name__] = {"read" : read, "write" : write, "convert" : convert}

def readType(cls, reader):
    return _type_registry[cls.__name__]["read"](reader)

def writeType(cls, writer, obj):
    if not isinstance(obj, cls):
        if _type_registry[cls.__name__]["convert"] is None:
            raise Exception("Object type is not correct %s != %s" % (type(obj).__name__, cls.__name__))
        try:
            obj = _type_registry[cls.__name__]["convert"](obj)
        except Exception as e:
            raise Exception("Unable to convert %s to %s: %s" % (type(obj).__name__, cls.__name__, e))
    _type_registry[cls.__name__]["write"](writer, obj)

def convertInt(obj):
    return int(obj)

def convertFloat(obj):
    return float(obj)

def convertStr(obj):
    return str(obj)

def convertBool(obj):
    return bool(obj)

registerType(int, lambda x: x.readInt(), lambda x, o: x.writeInt(o), convertInt)
registerType(long, lambda x: x.readLong(), lambda x, o: x.writeLong(o), convertInt)
registerType(float, lambda x: x.readFloat(), lambda x, o: x.writeFloat(o), convertFloat)
registerType(str, lambda x: x.readString(), lambda x, o: x.writeString(o), convertStr)
registerType(double, lambda x: x.readDouble(), lambda x, o: x.writeDouble(o), convertFloat)
registerType(char, lambda x: x.readChar(), lambda x, o: x.writeChar(o))
registerType(bool, lambda x: x.readBool(), lambda x, o: x.writeBool(o), convertBool)

import datetime

registerType(datetime.datetime, _echo.readTimestamp, _echo.writeTimestamp)

def readList(cls, reader):
    objects = []
    n = reader.readInt()
    read = _type_registry[cls.__name__]["read"]
    for i in range(n):
        objects.append(read(reader))
    return objects

def writeList(cls, writer, objects):
    writer.writeInt(len(objects))
    write = _type_registry[cls.__name__]["write"]
    for o in objects:
        write(writer, o)

class Dictionary(dict):

    def __init__(self, reader=None):
        super().__init__()
        if reader:
            n = reader.readInt()
            for _ in range(0, n):
                key = reader.readString()
                value = reader.readString()
                self[key] = value

    @staticmethod
    def read(reader):
        obj = Dictionary()
        n = reader.readInt()
        for _ in range(0, n):
            key = reader.readString()
            value = reader.readString()
            obj[key] = value
        return obj

    @staticmethod
    def write(writer, obj):
        writer.writeInt(len(obj))
        for k, v in obj.items():
            writer.writeString(k)
            writer.writeString(str(v))

    def pack(self):
        writer = _echo.MessageWriter(4 + sum([ len(k) + len(v) + 8 for k, v in self.items()]))
        self.write(writer)
        return writer

    def __setitem__(self, key, item):
        self.__dict__[key] = item

    def __getitem__(self, key):
        return self.__dict__[key]

    def __repr__(self):
        return repr(self.__dict__)

    def __len__(self):
        return len(self.__dict__)

    def __delitem__(self, key):
        del self.__dict__[key]

    def get(self, key, default = None):
        return self.__dict__.get(key, default)

    def clear(self):
        return self.__dict__.clear()

    def copy(self):
        return self.__dict__.copy()

    def has_key(self, k):
        return self.__dict__.has_key(k)

    def pop(self, key, default = None):
        return self.__dict__.pop(key, default)

    def update(self, *args, **kwargs):
        return self.__dict__.update(*args, **kwargs)

    def keys(self):
        return self.__dict__.keys()

    def values(self):
        return self.__dict__.values()

    def items(self):
        return self.__dict__.items()

    def __contains__(self, item):
        return item in self.__dict__

    def __iter__(self):
        return iter(self.__dict__)

registerType(Dictionary, Dictionary.read, Dictionary.write)

class Header(object):
    def __init__(self, source = "", timestamp = None):
        self.source = source
        if timestamp == None:
            self.timestamp = datetime.datetime.now()
        else:
            self.timestamp = timestamp

    @staticmethod
    def read(reader):
        return Header(readType(str, reader), readType(datetime.datetime, reader))

    @staticmethod
    def write(writer, obj):
        writeType(str, writer, obj.source)
        writeType(datetime.datetime, writer, obj.timestamp)

registerType(Header, Header.read, Header.write)

class DictionarySubscriber(_echo.Subscriber):

    def __init__(self, client, alias, callback):
        def _read(message):
            reader = _echo.MessageReader(message)
            return Dictionary.read(reader)

        super().__init__(client, alias, "dictionary", lambda x: callback(_read(x)))

class DictionaryPublisher(_echo.Publisher):

    def __init__(self, client, alias):
        super().__init__(client, alias, "dictionary")

    def send(self, obj):
        writer = _echo.MessageWriter()
        Dictionary.write(writer, obj)
        super().send(writer)

class SubscriptionWatcher(_echo.Watcher):

    def __init__(self, client, alias, callback=None, watch=True):
        super().__init__(client, alias)
        #self._callback = callback
        #self._subscribers = 0
        #if watch:
        #    self.watch()

    def on_event(self, message):
        print("event", message)
        #mtype = message.get("type", "")

        #if mtype == "subscribe" or mtype == "unsubscribe" or mtype == "summary":
        #    try:
        #        self._subscribers = int(message.get("subscribers", 0))
        #    except ValueError:
        #        self._subscribers = 0

            #if self._callback is not None:
            #    self._callback(self._subscribers)

    @property
    def subscribers(self):
        return self._subscribers
