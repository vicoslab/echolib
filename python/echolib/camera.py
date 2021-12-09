
import echolib
import numpy

try:
    from echolib import _echo
except ImportError as ie:
    raise ImportError("Echo binary library not found", ie)

class CameraIntrinsics(object):
    def __init__(self):
        self.width = None
        self.height = None
        self.intrinsics = None
        self.distortion = None
        self.name = 'Unknown'

    @staticmethod
    def read(reader):
        obj = CameraIntrinsics()
        obj.width = reader.readInt()
        obj.height = reader.readInt()
        obj.intrinsics = _echo.readTensor(reader)
        obj.distortion = _echo.readTensor(reader)
        return obj

    @staticmethod
    def write(writer, obj):
        writer.writeInt(obj.width)
        writer.writeInt(obj.height)
        _echo.writeTensor(writer, obj.intrinsics)
        _echo.writeTensor(writer, obj.distortion)

class CameraExtrinsics(object):

    def __init__(self, header = echolib.Header(), rotation=numpy.eye(3), translation=numpy.empty((1, 3))):
        self.header = header
        self.rotation = rotation
        self.translation = translation

    @staticmethod
    def read(reader):
        obj = CameraExtrinsics()
        obj.header = echolib.readType(echolib.Header, reader)
        obj.rotation = _echo.readTensor(reader)
        obj.translation = _echo.readTensor(reader)
        return obj

    @staticmethod
    def write(writer, obj):
        echolib.writeType(echolib.Header, writer, obj.header)
        _echo.writeTensor(writer, obj.rotation)
        _echo.writeTensor(writer, obj.translation)

echolib.registerType(numpy.array, _echo.readTensor, _echo.writeTensor)
echolib.registerType(CameraIntrinsics, CameraIntrinsics.read, CameraIntrinsics.write)
echolib.registerType(CameraExtrinsics, CameraExtrinsics.read, CameraExtrinsics.write)

class Frame(object):

    def __init__(self, header = echolib.Header(), image = numpy.array(())):
        self.header = header
        self.image = image

    @staticmethod
    def read(reader):
        obj = Frame()
        obj.header = echolib.readType(echolib.Header, reader)
        obj.image = _echo.readTensor(reader)
        return obj

    @staticmethod
    def write(writer, obj):
        echolib.writeType(echolib.Header, writer, obj.header)
        _echo.writeTensor(writer, obj.image)

echolib.registerType(Frame, Frame.read, Frame.write)

class CameraIntrinsicsSubscriber(echolib.Subscriber):

    def __init__(self, client, alias, callback):
        def _read(message):
            reader = echolib.MessageReader(message)
            return CameraIntrinsics.read(reader)

        super().__init__(client, alias, "camera intrinsics", lambda x: callback(_read(x)))


class CameraIntrinsicsPublisher(echolib.Publisher):

    def __init__(self, client, alias):
        super().__init__(client, alias, "camera intrinsics")

    def send(self, obj):
        writer = echolib.MessageWriter()
        CameraIntrinsics.write(writer, obj)
        super().send(writer)

class CameraExtrinsicsSubscriber(echolib.Subscriber):

    def __init__(self, client, alias, callback):
        def _read(message):
            reader = echolib.MessageReader(message)
            return CameraExtrinsics.read(reader)

        super().__init__(client, alias, "camera extrinsics", lambda x: callback(_read(x)))


class CameraIntrinsicsPublisher(echolib.Publisher):

    def __init__(self, client, alias):
        super().__init__(client, alias, "camera extrinsics")

    def send(self, obj):
        writer = echolib.MessageWriter()
        CameraExtrinsics.write(writer, obj)
        super().send(writer)

class FrameSubscriber(echolib.Subscriber):

    def __init__(self, client, alias, callback):
        def _read(message):
            reader = echolib.MessageReader(message)
            return Frame.read(reader)

        super().__init__(client, alias, "camera frame", lambda x: callback(_read(x)))

class FramePublisher(echolib.Publisher):

    def __init__(self, client, alias):
        super().__init__(client, alias, "camera frame")

    def send(self, obj):
        writer = echolib.MessageWriter()
        Frame.write(writer, obj)
        super().send(writer)