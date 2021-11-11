#include <echolib/datatypes.h>
#include <echolib/array.h>

namespace echolib {

typedef struct CameraIntrinsics {
    int width;
    int height;
    SharedTensor intrinsics;
    SharedTensor distortion; 
} CameraIntrinsics;

typedef struct CameraExtrinsics {
    Header header;
    SharedTensor rotation;
    SharedTensor translation;
} CameraExtrinsics;

typedef struct Frame {
    Header header;
    SharedTensor image;
} Frame;

template <> inline string get_type_identifier<CameraExtrinsics>() { return string("camera extrinsics"); }

template<> inline shared_ptr<Message> echolib::Message::pack<CameraExtrinsics>(const CameraExtrinsics &data)
{
    MessageWriter writer;

    write(writer, data.header);
    write(writer, data.rotation);
    write(writer, data.translation);

    return make_shared<BufferedMessage>(writer);
}

template<> inline shared_ptr<CameraExtrinsics> echolib::Message::unpack<CameraExtrinsics>(SharedMessage message)
{
    MessageReader reader(message);

    shared_ptr<CameraExtrinsics> result(new CameraExtrinsics());
    read(reader, result->header);
    read(reader, result->rotation);
    read(reader, result->translation);
    return result;
}

template <> inline string get_type_identifier<CameraIntrinsics>() { return string("camera intrinsics"); }

template<> inline shared_ptr<Message> echolib::Message::pack<CameraIntrinsics>(const CameraIntrinsics &data)
{
    MessageWriter writer;

    writer.write<int>(data.width);
    writer.write<int>(data.height);
    write(writer, data.intrinsics);
    write(writer, data.distortion);

    return make_shared<BufferedMessage>(writer);
}

template<> inline shared_ptr<CameraIntrinsics> echolib::Message::unpack<CameraIntrinsics>(SharedMessage message)
{
    MessageReader reader(message);

    shared_ptr<CameraIntrinsics> result(new CameraIntrinsics());
    result->width = reader.read<int>();
    result->height = reader.read<int>();
    read(reader, result->intrinsics);
    read(reader, result->distortion);
    return result;
}

template <> inline string get_type_identifier<Frame>() { return string("camera frame"); }


template<> inline shared_ptr<Message> echolib::Message::pack<Frame>(const Frame &data) {

    return make_shared<MultiBufferMessage>(initializer_list<SharedBuffer>{
        pack<Header>(data.header),
        pack<SharedTensor>(data.image)
    });

}

template<> inline shared_ptr<Frame> echolib::Message::unpack<Frame>(SharedMessage message) {
    MessageReader reader(message);

    shared_ptr<Frame> result(new Frame());
    read(reader, result->header);
    read(reader, result->image);
    return result;
}

}