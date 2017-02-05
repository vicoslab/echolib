/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <iostream>
#include <thread>
#include <string>
#include <mutex>

#include "client.h"
#include "datatypes.h"

using namespace std;
using namespace echolib;

#define DELIMITER "$ "

namespace echolib {

//template <> inline string get_type_identifier<echolib::Dictionary>() { return string("dictionary"); }


template <> inline string get_type_identifier<pair<string, string> >() { return string("string pair"); }

template<> inline shared_ptr<Message> Message::pack<pair<string, string> >(const pair<string, string> &data) {
    MessageWriter writer(data.first.size() + data.second.size() + 8);

    writer.write_string(data.first);
    writer.write_string(data.second);

    return make_shared<BufferedMessage>(writer);
}

template<> inline shared_ptr<pair<string, string> > Message::unpack<pair<string, string> >(SharedMessage message) {
    MessageReader reader(message);

    string user = reader.read_string();
    string text = reader.read_string();

    return make_shared<pair<string, string> >(user, text);
}

}

int main(int argc, char** argv) {

    SharedIOLoop loop = make_shared<IOLoop>();

    string address;
    if (argc > 1) {
        address = string(argv[1]);
    }

    //Connect to local socket based daemon
    SharedClient client = make_shared<echolib::Client>(address);
    loop->add_handler(client);

    string name;
    cout << "Enter your name\n";
    std::getline(std::cin, name);

    std::mutex mutex;

    function<void(shared_ptr<pair<string, string> >)> chat_callback = [&](shared_ptr<pair<string, string> > m) {
        const string name = m->first;
        const string message = m->second;
        std::cout<< name << ": " << message << std::endl;
    };

    function<void(int)> subscribe_callback = [&](int m) {
        std::cout << "Total subscribers: " << m << std::endl;
    };

    TypedSubscriber<pair<string, string> > sub(client, "chat", chat_callback);
    SubscriptionWatcher watch(client, "chat", subscribe_callback);
    TypedPublisher<pair<string, string> > pub(client, "chat");

    std::thread write([&]() {
        while (client->is_connected()) {
            string message;
            std::getline(std::cin, message);
            pub.send(pair<string, string>(name, message));
        }

    });

    while(loop->wait(10)) {
        // We have to give the write thread some space
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    exit(0);
}
