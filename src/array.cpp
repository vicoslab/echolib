#include <iostream>
#include <thread>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <type_traits>

#include "debug.h"
#include <echolib/array.h>

using namespace std;
using namespace echolib;

namespace echolib {

Array::Array(): Array(0) {}

Array::Array(size_t size): size(size), data((uchar*) malloc(size)), callback(nullptr) {

}

Array::Array(size_t size, uchar* data, DescructorCallback callback): size(size), data(data), callback(callback) {

}

Array::~Array() {

    if (callback) {
        callback();
    } else {
        //std::cout << "Free arr " << this << " " << (void *) data << std::endl;
        if (data) free(data);
    }

    data = nullptr;
}

size_t Array::get_size() const {
    return size;
}

uchar* Array::get_data() const {
    return data;
}

size_t multiply_dimensions(echolib::any_container<size_t> dims) {

    if (dims->size() == 0) return 0;

    size_t count = 1;

    for (vector<size_t>::const_iterator it = dims->cbegin(); it != dims->cend(); it++) {
        count *= *it;
    }

    return count;
}

size_t Tensor::get_type_bytes(DataType dtype) {

    switch (dtype) {
        case UINT8:
        case INT8: {
            return 1;
        }
        case UINT16:
        case INT16: {
            return 2;
        }
        case UINT32:
        case INT32: {
            return 4;
        }
        case FLOAT32: {
            return 4;
        }
        case FLOAT64: {
            return 8;
        }
    }

    throw runtime_error("Unsupported data type");
}


Tensor::Tensor() : Tensor({}, UINT8) {}

Tensor::Tensor(echolib::any_container<size_t> dims, DataType dtype) : Array(multiply_dimensions(dims) * Tensor::get_type_bytes(dtype)), dimensions(dims->begin(), dims->end()), dtype(dtype)  {

}


Tensor::Tensor(echolib::any_container<size_t> dims, DataType dtype, uchar* data, DescructorCallback callback) : Array(multiply_dimensions(dims) * Tensor::get_type_bytes(dtype), data, callback), dimensions(dims->begin(), dims->end()), dtype(dtype) {

}

Tensor::~Tensor() {

}

#ifdef __ECHOLIB_HAS_OPENCV

#ifndef CV_MAX_DIM
const int CV_MAX_DIM = 32;
#endif

DataType Tensor::decode_ocvtype(int cvtype) {

    int depth = CV_MAT_DEPTH(cvtype);

    DataType dtype;

    switch (depth) {
        case CV_8U: { dtype = UINT8; break; }
        case CV_8S: { dtype = INT8; break; }
        case CV_16U: { dtype = UINT16; break; }
        case CV_16S: { dtype = INT16; break; }
        case CV_32S: { dtype = INT32; break; }
        case CV_32F: { dtype = FLOAT32; break; }
        case CV_64F: { dtype = FLOAT64; break; }
        default: {
            throw runtime_error("Unsupported data type");
        }
    }

    return dtype;
}

int Tensor::encode_ocvtype(DataType dtype) {

    int type = 0;

    switch (dtype) {
        case UINT8: { type = CV_8U; break; }
        case INT8: { type = CV_8S; break; }
        case UINT16: { type = CV_16U; break; }
        case INT16: { type = CV_16S; break; }
        case INT32: { type = CV_32S; break; }
        case FLOAT32: { type = CV_32F; break; }
        case FLOAT64: { type = CV_64F; break; }
        default: {
            throw runtime_error("Unsupported data type");
        }
    }

    return type;
}


Tensor::Tensor(cv::Mat& source) {

    if (!source.isContinuous()) 
        source = source.clone(); // Copy data in case of strides

    data = source.data;

    cv::Mat* reservation = new cv::Mat(source); // A dynamically allocated Mat, used to keep data in memory.
    callback = [reservation](){ delete reservation; };

    //int depth = CV_MAT_DEPTH(source.type());
    int cn = CV_MAT_CN(source.type());

    dtype = decode_ocvtype(source.type());

    dimensions = vector<size_t>(source.dims + (cn > 1 ? 1 : 0));

    for (int i = 0; i < source.dims; i++) {
        dimensions[i] = source.size[i];
    }

    if (cn > 1)
        dimensions[source.dims] = cn;

    size = 0;

    if (dimensions.size() > 0) {
        size = 1;
        for (size_t d : dimensions) { size *= d; }
    }

    size *= get_type_bytes(dtype);

    data = source.data;

}

cv::Mat Tensor::asMat() const {

    int type = encode_ocvtype(dtype);

    int ndims = (int) dimensions.size();

    int size[CV_MAX_DIM + 1];
    for (size_t i = 0; i < dimensions.size(); i++) { size[i] = (int) dimensions[i]; }

    if (ndims >= CV_MAX_DIM) {
        throw runtime_error(format_string("Dimensionality (%d) is not supported by OpenCV", ndims));
    } 

    if( ndims == 3 && size[2] <= CV_CN_MAX) {
        ndims--;
        type |= CV_MAKETYPE(0, size[2]);
    }

    return cv::Mat(ndims, size, type, data);

}

#endif

size_t Tensor::shape(size_t i) const {
    return dimensions[i];
}

uchar Tensor::ndims() const {
    return (uchar) dimensions.size();
}

const std::vector<size_t> Tensor::dims() const {
    return move(dimensions);
}

DataType Tensor::get_type() const {
    return dtype;
}

uchar Tensor::get_bytes() {
    return Tensor::get_type_bytes(dtype);
}

ArrayBuffer::ArrayBuffer(SharedArray array, function<void()> complete) : array(array), complete(complete) {}

ArrayBuffer::~ArrayBuffer() {
    if (complete)
        complete();
}

size_t ArrayBuffer::get_length() const {
    return array->get_size();
}

size_t ArrayBuffer::copy_data(size_t position, uchar* buffer, size_t length) const {
    length = min(length, array->get_size() - position);
    if (length < 1) return 0;

    memcpy(buffer, &(array->get_data()[position]), length);
    return length;
}
/*
Frame::Frame(Header header, Mat image): header(header), image(image) {}

Frame::~Frame() {}
*/
}
