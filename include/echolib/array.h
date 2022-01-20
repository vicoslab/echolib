#ifndef __ECHOLIB_ARRAY_H
#define __ECHOLIB_ARRAY_H

#include <echolib/client.h>
#include <echolib/datatypes.h>

using namespace std;
using namespace echolib;

#ifdef CV_VERSION_MAJOR
#define __ECHOLIB_HAS_OPENCV 1
#endif


namespace echolib {

enum DataType { UINT8 = 1, UINT16 = 2, UINT32 = 3, INT8 = 4, INT16 = 5, INT32 = 6, FLOAT32 = 10, FLOAT64 = 11 };

class ArrayBuffer;

typedef std::function<void()> DescructorCallback;

class Array {
public:

    Array();

    Array(size_t size);
    Array(size_t size, uchar* data, DescructorCallback callback);
    virtual ~Array();

    virtual size_t get_size() const;
    uchar* get_data() const;

protected:

    size_t size;
    uchar* data;
    DescructorCallback callback;

};

typedef shared_ptr<Array> SharedArray;

template <> inline string get_type_identifier<SharedArray>() { return string("array"); }

class Tensor: public Array {
public:

    Tensor();

    Tensor(echolib::any_container<size_t> dimensions, DataType dtype = UINT8);

    Tensor(echolib::any_container<size_t> dimensions, DataType dtype, uchar* data, DescructorCallback callback);

#ifdef __ECHOLIB_HAS_OPENCV
    static DataType decode_ocvtype(int cvtype);

    static int encode_ocvtype(DataType dtype);

    Tensor(cv::Mat& source);

    template<typename T, int m, int n> Tensor(const cv::Matx<T, m, n>& source) : Tensor({source.rows, source.cols}, decode_ocvtype(cv::DataType<T>::depth)) {
        
        T* ptr = (T*) get_data();

        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                ptr[0] = source(i, j);
                ptr++;
            }
        }

    }

    template<typename T, int m, int n> cv::Matx<T, m, n> asMat() const {
        
        int reftype = CV_MAKETYPE(cv::DataType<T>::depth, 1);

        assert(ndims() == 2 && m == shape(0) && n == shape(1) && encode_ocvtype(dtype) == reftype);

        T* ptr = (T*) get_data();

        cv::Matx<T, m, n> dst;

        size_t k = 0;

        for (int i = 0; i < m; i++) {
            for (int j = 0; j < n; j++) {
                dst(i, j) = data[k];
                k++;
            }
        }

        return dst;

    }

    cv::Mat asMat() const;

#endif

    virtual ~Tensor();

    DataType get_type() const;

    size_t shape(size_t i) const;

    const std::vector<size_t> dims() const;

    uchar ndims() const;

    uchar get_bytes();

    static size_t get_type_bytes(DataType dtype);

protected:

    vector<size_t> dimensions;

    DataType dtype;

};

typedef shared_ptr<Tensor> SharedTensor;

template <> inline string get_type_identifier<SharedTensor>() { return string("tensor"); }

class ArrayBuffer : public Buffer {
public:
    ArrayBuffer(SharedArray array, function<void()> complete = NULL);

    virtual ~ArrayBuffer();

    virtual size_t get_length() const;

    virtual size_t copy_data(size_t position, uchar* buffer, size_t length) const;

private:

    const SharedArray array;
    function<void()> complete;

};

template<> inline void read(MessageReader& reader, SharedArray& dst) {

    int size = reader.read<size_t>(); 

    dst = make_shared<Array>(size);

    reader.copy_data(dst->get_data(), size);

}

template<> inline void write(MessageWriter& writer, const SharedArray& src) {
    
    writer.write<size_t>(src->get_size());

    writer.write_buffer(src->get_data(), src->get_size());

}

template<> inline void read(MessageReader& reader, SharedTensor& dst) {

    uint8_t ndim = reader.read<size_t>(); 

    std::vector<size_t> dimensions;

    for (size_t i = 0; i < ndim; i++) {
        dimensions.push_back(reader.read<size_t>());
    }

    DataType type = (DataType) reader.read<uint8_t>(); 

    dst = make_shared<Tensor>(dimensions, type);

    reader.copy_data(dst->get_data(), dst->get_size());

}

template<> inline void write(MessageWriter& writer, const SharedTensor& src) {
    
    writer.write<size_t>((size_t) src->ndims());

    for (size_t i = 0; i < src->ndims(); i++) {
        writer.write<size_t>(src->shape(i));
    }

    writer.write<uint8_t>((uint8_t) src->get_type());

    writer.write_buffer(src->get_data(), src->get_size());

}

template<>
inline shared_ptr<SharedArray> Message::unpack(SharedMessage message) {

    MessageReader reader(message);

    shared_ptr<SharedArray> array(new SharedArray);

    read(reader, *array);

    return array;
}

template<>
inline shared_ptr<Message> Message::pack(const SharedArray &data) {

    ssize_t length = message_length(data);

    MessageWriter writer(length);

    write(writer, data);

    return make_shared<BufferedMessage>(writer);
}


template<>
inline shared_ptr<SharedTensor> Message::unpack(SharedMessage message) {

    MessageReader reader(message);

    shared_ptr<SharedTensor> out(new SharedTensor);

    read(reader, *out);

    return out;
}

template<>
inline shared_ptr<Message> Message::pack(const SharedTensor &data) {

    return make_shared<MultiBufferMessage>(std::initializer_list<SharedBuffer>{
        ListBuffer<size_t>::wrap(data->dims()),
        PrimitiveBuffer<uchar>::wrap(data->get_type()),
        make_shared<ArrayBuffer>(data)
    });

}

}

#endif