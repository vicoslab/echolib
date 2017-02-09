 
from echolib.pyecho import *

double = type('double', (), {})
char = type('char', (), {})

_type_registry = {}

def registerType(cls, read, write):
    if cls.__name__ in _type_registry:
        raise Exception("Type %s already registered" % cls.__name__)
    _type_registry[cls.__name__] = {"read" : read, "write" : write}

def readType(cls, reader):
    return _type_registry[cls.__name__]["read"](reader)

def writeType(cls, writer, obj):
    if not isinstance(obj, cls):
        raise Exception("Object type is not correct %s != %s" % (type(obj).__name__, cls.__name__))
    _type_registry[cls.__name__]["write"](writer, obj)

registerType(int, lambda x: x.readInt(), lambda x, o: x.writeInt(o))
registerType(long, lambda x: x.readLong(), lambda x, o: x.writeLong(o))
registerType(float, lambda x: x.readFloat(), lambda x, o: x.writeFloat(o))
registerType(str, lambda x: x.readString(), lambda x, o: x.writeString(o))
registerType(double, lambda x: x.readDouble(), lambda x, o: x.writeDouble(o))
registerType(char, lambda x: x.readChar(), lambda x, o: x.writeChar(o))
registerType(bool, lambda x: x.readBool(), lambda x, o: x.writeBool(o))

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
        super(Dictionary, self).__init__()
        if reader:
            n = reader.readInt()
            for i in xrange(0, n):
                key = reader.readString()
                value = reader.readString()
                self[key] = value

    @staticmethod
    def read(reader):
    	obj = Dictionary()
        n = reader.readInt()
        for i in xrange(0, n):
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
        writer = echolib.pyecho.MessageWriter(4 + sum([ len(k) + len(v) + 8 for k, v in self.items()]))
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
        return self.__dict__.pop(k, d)

    def update(self, *args, **kwargs):
        return self.__dict__.update(*args, **kwargs)

    def keys(self):
        return self.__dict__.keys()

    def values(self):
        return self.__dict__.values()

    def items(self):
        return self.__dict__.items()

    def pop(self, *args):
        return self.__dict__.pop(*args)

    def __cmp__(self, dict):
        return cmp(self.__dict__, dict)

    def __contains__(self, item):
        return item in self.__dict__

    def __iter__(self):
        return iter(self.__dict__)

    def __unicode__(self):
        return unicode(repr(self.__dict__))

registerType(Dictionary, Dictionary.read, Dictionary.write)


class DictionarySubscriber(Subscriber):

    def __init__(self, client, alias, callback):
        def _read(message):
            reader = MessageReader(message)
            return Dictionary.read(reader)

        super(DictionarySubscriber, self).__init__(client, alias, "dictionary", lambda x: callback(_read(x)))

class DictionaryPublisher(Publisher):

    def __init__(self, client, alias):
        super(DictionaryPublisher, self).__init__(client, alias, "dictionary")

    def send(self, obj):
        writer = MessageWriter()
        Dictionary.write(writer, obj)
        super(DictionaryPublisher, self).send(writer)
