/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <iostream>
#include <thread>
#include <string>
#include <iostream>
#include <fstream>

#include "client.h"
#include "datatypes.h"

using namespace std;
using namespace echolib;

#define DELIMITER "$ "

namespace echolib {

template <> inline string get_type_identifier<string>() { return string("string"); }

template<> inline shared_ptr<Message> echolib::Message::pack<string>(const string &data) {
    MessageWriter writer(data.size() + sizeof(int));

    writer.write_string(data);

    return make_shared<BufferedMessage>(writer);
}

template<> inline shared_ptr<string> echolib::Message::unpack<string>(SharedMessage message) {

    MessageReader reader(message);

    string data = reader.read_string();

    return make_shared<string>(data);
}

}

inline std::string slurp (const std::string& path) {
    std::ostringstream buf;
    std::ifstream input (path.c_str());
    buf << input.rdbuf();
    return buf.str();
}

int main(int argc, char** argv) {

    SharedIOLoop loop = make_shared<IOLoop>();

    SharedClient client = make_shared<echolib::Client>(string(argv[1]));

    loop->add_handler(client);

    if (argc > 2) {

        bool write = false;

        string data = slurp(string(argv[2]));

        function<void(int)> subscribe_callback = [&](int m) {
            write = m > 0;
        };

        TypedPublisher<string,true> pub(client, "chunked");
        SubscriptionWatcher watch(client, "chunked", subscribe_callback);

        int counter = 0;

        while(loop->wait(10)) {
            counter++;
            if (write && counter == 50) {
                std::cout << "Writing data" << std::endl;
                pub.send(data);
            }
            counter = counter % 50;
        }

    } else {
        function<void(shared_ptr<string>)> chunked_callback = [&](shared_ptr<string> m) {

            std::cout << "Received " << m->size() << " bytes" << std::endl;

        };

        TypedSubscriber<string, true> sub(client, "chunked", chunked_callback);

        while(loop->wait(10)) {
        }

    }

    exit(0);
}
