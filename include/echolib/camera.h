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

/*
template<typename T, int m, int n> void write(MessageWriter& writer, const Matx<T, m, n>& src) {
    
    assert(m == src.rows && n == src.cols);

    writer.write<ushort>(src.rows);
    writer.write<ushort>(src.cols);
    writer.write<int>(CV_MAKETYPE(DataType<T>::depth, 1));

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            writer.write<T>(src(i, j));
        }
    }

}

template<typename T, int m, int n> void read(MessageReader& reader, Matx<T, m, n>& dst) {
    
    ushort h = reader.read<ushort>();
    ushort w = reader.read<ushort>();
    int t = reader.read<int>();

    assert(m == h && n == w && t == CV_MAKETYPE(DataType<T>::depth, 1));

    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            dst(i, j) = reader.read<T>();
        }
    }

}*/

template <> inline string get_type_identifier<CameraExtrinsics>() { return string("camera extrinsics"); }

template<> inline shared_ptr<Message> echolib::Message::pack<CameraExtrinsics>(const CameraExtrinsics &data)
{
    MessageWriter writer(12 * sizeof(float));

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
    MessageWriter writer(12 * sizeof(float));

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

    MessageWriter writer;
    write(writer, data.header);

    return make_shared<MultiBufferMessage>(initializer_list<SharedBuffer>{
        make_shared<BufferedMessage>(writer),
        ListBuffer<size_t>::wrap(data.image->dims()),
        PrimitiveBuffer<uint8_t>::wrap((uint8_t)data.image->get_type()),
        make_shared<ArrayBuffer>(data.image)
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