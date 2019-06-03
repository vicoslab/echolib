/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <iostream>
#include <algorithm>
#include <string>
#include <thread>
#include <chrono>

#include <echolib/client.h>
#include <echolib/datatypes.h>

using namespace std;
using namespace echolib;

#define DELIMITER "$ "

namespace echolib {


template <> inline string get_type_identifier<string>() { return string("string"); }

template<> inline shared_ptr<Message> Message::pack<string>(const string &data) {
    MessageWriter writer(data.size() + 4);

    writer.write_string(data);

    return make_shared<BufferedMessage>(writer);
}

template<> inline shared_ptr<string> Message::unpack<string>(SharedMessage message) {
    MessageReader reader(message);

    string data = reader.read_string();

    return make_shared<string>(data);
}

}

std::string random_string( size_t length )
{
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };
    std::string str(length,0);
    std::generate_n( str.begin(), length, randchar );
    return str;
}

int main(int argc, char** argv) {

    SharedClient client = echolib::connect();

    if (argc > 1) {

        float frequency = max(0.00001, min(500.0, atof(argv[1])));

        function<void(shared_ptr<string>)> callback = [&](shared_ptr<string> m) {
            std::cout  << "Received " << (*m).size() << " bytes" << std::endl;
        };


        TypedSubscriber<string> sub(client, "deadend", callback);

        while(echolib::wait(1)) {
            // We have to give the write thread some space
            std::this_thread::sleep_for(std::chrono::milliseconds((int)( 1000.0 / frequency)));
        }

    } else {

        TypedPublisher<string> pub(client, "deadend");


        while(echolib::wait(10)) {
            string data = random_string(1000);
            pub.send(data);
        }

    }

    exit(0);
}
