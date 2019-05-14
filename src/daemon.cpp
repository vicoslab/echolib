/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <stdlib.h>

#include "debug.h"
#include "loop.h"
#include "routing.h"
#include "debug.h"

using namespace echolib;
using namespace std;

// https://stackoverflow.com/questions/8104904/identify-program-that-connects-to-a-unix-domain-socket

int main(int argc, char *argv[]) {

    SharedIOLoop loop = make_shared<IOLoop>();

    string address;
    if (argc > 1) {
        address = string(argv[1]);
    }

    shared_ptr<Router> router = make_shared<Router>(loop, address);
    loop->add_handler(router);

    while (true) {

        loop->wait(5000);

        DEBUGGING {
            cout << " --------------------------- Daemon statistics --------------------------------- " <<  endl;
            router->print_statistics();
        }

    }

    return EXIT_SUCCESS;
}
