/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_LOOP_HPP_
#define ECHO_LOOP_HPP_

#include <string>
#include <map>
#include <utility>
#include <memory>
#include <memory>

#include "message.h"

using namespace std;

namespace echolib {

class IOBase {
public:
	virtual ~IOBase() {};

	virtual int get_file_descriptor() = 0;

	virtual bool handle() = 0;

	virtual void disconnect() = 0;

};

typedef shared_ptr<IOBase> SharedIOBase;

class IOLoop {
public:
	IOLoop();
	virtual ~IOLoop();

	virtual void add_handler(SharedIOBase base);

	virtual void remove_handler(SharedIOBase base);

	virtual bool wait(long timeout = -1);

private:

	map<int, SharedIOBase> handlers;

	int efd;

};

}

#endif