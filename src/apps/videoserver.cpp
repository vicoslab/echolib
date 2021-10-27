#include <unistd.h>

#include <iostream>
#include <fstream>
#include <memory>
#include <opencv2/opencv.hpp>

#include <echolib/client.h>
#include <echolib/datatypes.h>
#include <echolib/helpers.h>
#include <echolib/camera.h>

using namespace std;
using namespace echolib;
using namespace cv;

CameraIntrinsics parameters;

#define READ_MATX(N, M) { Mat tmp; (N) >> tmp; (M) = tmp; }

int main(int argc, char** argv) {

    string filename(argv[1]);

    SharedClient client = echolib::connect(string(), "videoserver");

    Mat image, image_rgb;

    VideoCapture video(filename);

    if (!video.isOpened()) {
        cerr << "Cannot open image file " << filename << endl;
        return -1;
    }
    
    Matx33f intrinsics;
    Mat distortion;

    FileStorage fsc("calibration.xml", FileStorage::READ);
    if (fsc.isOpened()) {
        READ_MATX(fsc["intrinsic"], intrinsics);
        fsc["distortion"] >> distortion;
    } else {
        intrinsics(0, 0) = 700;
        intrinsics(1, 1) = 700;
        intrinsics(0, 2) = video.get(CAP_PROP_FRAME_WIDTH) / 2;
        intrinsics(1, 2) = video.get(CAP_PROP_FRAME_HEIGHT) / 2;
        intrinsics(2, 2) = 1;
        distortion = (Mat_<float>(1,5) << 0, 0, 0, 0, 0);
    }

    parameters.intrinsics = make_shared<Tensor>(intrinsics);
    parameters.distortion = make_shared<Tensor>(distortion);

    parameters.width = video.get(CAP_PROP_FRAME_WIDTH);
    parameters.height = video.get(CAP_PROP_FRAME_HEIGHT);

    SharedTypedPublisher<Frame> frame_publisher = make_shared<TypedPublisher<Frame> >(client, "camera", 1);

    SubscriptionWatcher watcher(client, "camera");

    StaticPublisher<CameraIntrinsics> intrinsics_publisher = StaticPublisher<CameraIntrinsics>(client, "intrinsics", parameters);

    while (true) {
        video >> image;

        if (image.empty()) {
            video.set(CAP_PROP_POS_FRAMES, 0);
            continue;
        }

        cv::cvtColor(image, image_rgb, COLOR_BGR2RGB);

        std::chrono::system_clock::time_point a = std::chrono::system_clock::now();

        if (watcher.get_subscribers() > 0) {
            Frame frame{Header("video", a), make_shared<Tensor>(image_rgb)};

            frame_publisher->send(frame);
        }
        if (!echolib::wait(30)) break;
    }

    exit(0);
}
