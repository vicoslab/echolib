 
from echolib.pyecho import *

double = type('double', (), {})
char = type('char', (), {})

_type_registry = {}

def registerType(cls, read, write):
	_type_registry[cls.__name__] = {"read" : read, "write" : write}

def readType(cls, reader):
	return _type_registry[cls.__name__]["read"](reader)

def writeType(cls, writer, obj):
	if not instanceof(obj, cls):
		raise Exception("Object type is not correct")
	_type_registry[cls.__name__]["write"](writer, obj)

registerType(int, lambda x: x.readInt(), lambda x, o: x.writeInt(o))
registerType(long, lambda x: x.readLong(), lambda x, o: x.writeLong(o))
registerType(float, lambda x: x.readFloat(), lambda x, o: x.writeFloat(o))
registerType(str, lambda x: x.readString(), lambda x, o: x.writeString(o))
registerType(double, lambda x: x.readDouble(), lambda x, o: x.writeDouble(o))
registerType(char, lambda x: x.readChar(), lambda x, o: x.writeChar(o))

def readList(reader, cls):
	objects = []
	n = reader.readInt()
	read = _type_registry[cls.__name__][read]
	for i in range(n):
		objects.append(read(reader))
	return objects

def writeList(writer, cls, objects):
	writer.writeInt(len(objects))
	write = _type_registry[cls.__name__]["write"]
	for o in objects:
		write(writer, o)
