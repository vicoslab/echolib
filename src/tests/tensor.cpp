


#include <iostream>
#include <fstream>
#include <memory>

#include <echolib/client.h>
#include <echolib/datatypes.h>
#include <echolib/helpers.h>
#include <echolib/array.h>

using namespace std;
using namespace echolib;

SharedTensor frame;

void handle_frame(shared_ptr<SharedTensor> data) {

    bool same = true;

    SharedTensor out = *data;

    if (frame->get_size() == out->get_size()) {

        uint8_t* a = (uint8_t *) out->get_data();
        uint8_t* b = (uint8_t *) frame->get_data();

        for (size_t i = 0; i < frame->get_size(); i++) {
            if (a[i] != b[i]) {
                same = false;
                break;
            }
        }

    } else same = false;

	exit(same ? 0 : -1);

}

int main(int argc, char** argv) {

    SharedClient client = echolib::connect();

    frame = make_shared<Tensor>(initializer_list<size_t>{100, 100}, UINT8);

	uint8_t* data = (uint8_t *) frame->get_data();

    for (size_t i = 0; i < frame->get_size(); i++) {
        data[i] = i % 256;
    }

    auto image_publisher = make_shared<TypedPublisher<SharedTensor>>(client, "image");
	TypedSubscriber<SharedTensor> sub(client, "image", handle_frame);

    while (true) {

        image_publisher->send(frame);

        if (!echolib::wait(100)) break;
    }

    exit(-2);
}
