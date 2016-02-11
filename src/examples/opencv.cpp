/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <iostream>
#include <thread>
#include <fstream>
#include <cmath>
#include <opencv2/opencv.hpp>


#include "client.h"
#include "datatypes.h"

using namespace std;
using namespace echolib;
using namespace cv;


class ImageSubscriber : public ChunkedSubscriber {
  public:
    ImageSubscriber(SharedClient client, const string &alias, function<void(Mat&)> callback) : ChunkedSubscriber(client, alias, string("opencv matrix")), callback(callback) {

    }

    virtual ~ImageSubscriber() {

    }

    virtual void on_message(SharedMessage message) {

        try {

            MessageReader reader(message);
            int type = reader.read_integer();
            int cols = reader.read_integer();
            int rows = reader.read_integer();

            Mat data(rows, cols, type);

            reader.copy_data(data.data, data.cols * data.rows * data.elemSize());

            callback(data);

        } catch (echolib::ParseException &e) {
            Subscriber::on_error(e);
        }

    };

  private:

    function<void(Mat&)> callback;

};

class ImagePublisher : public ChunkedPublisher {
  public:
    ImagePublisher(SharedClient client, const string &alias) : ChunkedPublisher(client, alias, string("opencv matrix")) {

    }

    virtual ~ImagePublisher() {

    }

    bool send(Mat &mat) {

        shared_ptr<MemoryBuffer> header = make_shared<MemoryBuffer>(3 * sizeof(int));
        MessageWriter writer(header->get_buffer(), header->get_length());
        //writer.write_integer(mat.elemSize());
        writer.write_integer(mat.type());
        writer.write_integer(mat.cols);
        writer.write_integer(mat.rows);

        vector<SharedBuffer> buffers;
        buffers.push_back(header);
        buffers.push_back(make_shared<MatBuffer>(mat));

        shared_ptr<Message> message = make_shared<MultiBufferMessage>(get_channel_id(), buffers);

        return send_message_internal(message);

    }

  private:

    class MatBuffer : public Buffer {

      public:
        MatBuffer(Mat& mat) : mat(mat) {
            mat_length =  mat.cols * mat.rows * mat.elemSize();
        }
        virtual ~MatBuffer() {};

        virtual ssize_t get_length() const {
            return mat_length;
        }

        virtual ssize_t copy_data(ssize_t position, uchar* buffer, ssize_t length) const {
            length = min(length, mat_length - position);
            if (length < 1) return 0;

            memcpy(buffer, &(mat.data[position]), length);
            return length;
        }

      private:
        Mat mat;
        ssize_t mat_length;
    };

};

int main(int argc, char** argv) {
    SharedClient client = make_shared<echolib::Client>(string(argv[1]));
    if (argc > 2) {

        bool write = false;

        VideoCapture device(atoi(argv[2]));

        function<void(int)> subscribe_callback = [&](int m) {
            write = m > 0;
        };

        ImagePublisher pub(client, "images");
        SubscriptionWatcher watch(client, "images", subscribe_callback);
        while (true) {
            timespec spec;
            clock_gettime(CLOCK_REALTIME,&spec);
            long start_time = clock();

            Mat frame;
            device >> frame;
            if (write) {
                pub.send(frame);
            }
            long time = std::max(1L, 100 - (((clock() - start_time) * 1000) / CLOCKS_PER_SEC));
            printf("Time %ldms (queue size %d)  \n", time, client->get_queue_size());
            if (!client->wait(time)) break;
        }

    } else {
        namedWindow("Image");

        function<void(Mat&)> image_callback = [&](Mat& m) {

            imshow("Image", m);

        };

        ImageSubscriber sub(client, "images", image_callback);

        while(client->wait(100)) {

            waitKey(1);

        }

    }

    exit(0);
}
