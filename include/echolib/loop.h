/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#ifndef ECHO_LOOP_HPP_
#define ECHO_LOOP_HPP_

#include <string>
#include <map>
#include <vector>
#include <utility>
#include <memory>
#include <mutex>

#include <echolib/message.h>

#define SYNCHRONIZED(M) std::lock_guard<std::recursive_mutex> lock (M)

using namespace std;

namespace echolib {

class IOBase;
typedef shared_ptr<IOBase> SharedIOBase;

class IOBaseObserver {
public:

    virtual void on_output(SharedIOBase) = 0;

};

typedef std::shared_ptr<IOBaseObserver> SharedIOBaseObserver;

class IOBase : public std::enable_shared_from_this<IOBase> {
public:
	virtual ~IOBase() {};

	virtual int get_file_descriptor() = 0;

	virtual bool handle_input() = 0;

	virtual bool handle_output() = 0;

	virtual void disconnect() = 0;

    bool observe(SharedIOBaseObserver observer);
    bool unobserve(SharedIOBaseObserver observer);

protected:

	void notify_output();

    std::recursive_mutex mutex;

private:
    vector<SharedIOBaseObserver> observers;
};


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

	virtual bool wait(int64_t timeout = -1);

private:

	class IOLoopWriteObserver: public IOBaseObserver {
	public:
		IOLoopWriteObserver(IOLoop* context): context(context), flag(false) {};

		~IOLoopWriteObserver() {};

		virtual void on_output(SharedIOBase base);

		void clear(SharedIOBase base);

	private:

		IOLoop* context;

		bool flag;

	};

	map<int, SharedIOBase> handlers;

	map<int, std::shared_ptr<IOLoopWriteObserver> > observers;

	int efd;

};

SharedIOLoop default_loop();

bool wait(int64_t timeout = -1);
	

}

#endif