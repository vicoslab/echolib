/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_LOOP_HPP_
#define ECHO_LOOP_HPP_

#include <string>
#include <map>
#include <vector>
#include <utility>
#include <memory>

#include "message.h"

using namespace std;

namespace echolib {

class IOBase {
public:
	virtual ~IOBase() {};

	virtual int get_file_descriptor() = 0;

	virtual bool handle_input() = 0;

	virtual bool handle_output() = 0;

	virtual void disconnect() = 0;

};

typedef shared_ptr<IOBase> SharedIOBase;

/*
class IOBufferedBase : class IOBase {
public:
	virtual ~IOBase() {};

	virtual int get_file_descriptor() = 0;

	virtual bool handle();

	virtual void disconnect() = 0;

private:

	vector

};
*/

class IOLoop;
typedef shared_ptr<IOLoop> SharedIOLoop;

class IOLoop : public enable_shared_from_this<IOLoop> {
public:
	IOLoop();
	virtual ~IOLoop();

	virtual void add_handler(SharedIOBase base);

	virtual void remove_handler(SharedIOBase base);

	virtual bool wait(long timeout = -1);

	//static SharedIOLoop default_loop();

private:

	//static SharedIOLoop loop;

	map<int, SharedIOBase> handlers;

	int efd;

};

SharedIOLoop default_loop();

bool wait(long timeout = -1);
	

}

#endif