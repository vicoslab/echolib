/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <iostream>
#include <algorithm>
#include <string>
#include <thread>
#include <chrono>

#include "client.h"
#include "datatypes.h"

using namespace std;
using namespace echolib;

int main(int argc, char** argv) {

    SharedIOLoop loop = make_shared<IOLoop>();

    for (int i = 0; i < 100; i++) {

        SharedClient client = make_shared<echolib::Client>();

        cout << "Adding handler" << endl;

        loop->add_handler(client);
        
        loop->wait(100);

        cout << "Removing handler" << endl;

        loop->remove_handler(client);

        loop->wait(100);

    }

    exit(0);
}
