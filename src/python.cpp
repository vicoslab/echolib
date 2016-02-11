/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <vector>

PYBIND11_DECLARE_HOLDER_TYPE(T, std::shared_ptr<T>)

#include "client.h"


using namespace echolib;
namespace py = pybind11;

typedef function<void(string)> MessageCallback;

class PySubscriber : public Subscriber, public std::enable_shared_from_this<PySubscriber> {
  public:
    PySubscriber(SharedClient client, const string &alias, const string &type, function<void(MessageReader)> callback) : Subscriber(client, alias, type), callback(callback) {

    }

    virtual void on_message(SharedMessage message) {
        MessageReader reader(message);
        py::gil_scoped_acquire gil; // acquire GIL lock
        callback(reader);
    }

  private:

    function<void(MessageReader)> callback;

};

class PyWatcher : public Watcher {
  public:
    using Watcher::Watcher;

    virtual void on_event(SharedDictionary command) {
        PYBIND11_OVERLOAD_PURE(
            void,
            Subscriber,
            on_event,
            command
        );
    }
};

PYBIND11_PLUGIN(pyecho) {

    py::init_threading();
    py::module m("pyecho", "Echo IPC library");

    py::class_<Client, std::shared_ptr<Client> >(m, "Client")
    .def(py::init<string>())
    .def("disconnect", &Client::disconnect, "Disconnect the client")
    .def("wait", [](Client& c, long timeout) {
        py::gil_scoped_release gil; // release GIL lock
        return c.wait(timeout);
    }, "Wait for more messages")
    .def("isConnected", &Client::is_connected, "Check if the client is connected");

    py::class_<PySubscriber, std::shared_ptr<PySubscriber> >(m, "Subscriber")
    .def(py::init<SharedClient, string, string, function<void(MessageReader)> >())
    .def("subscribe", [](PySubscriber &a) {
        py::gil_scoped_release gil; // release GIL lock
        return a.subscribe();
    }, "Start receiving")
    .def("unsubscribe", [](PySubscriber &a) {
        py::gil_scoped_release gil; // release GIL lock
        return a.unsubscribe();
    }, "Stop receiving");

    py::class_<PyWatcher, std::shared_ptr<PyWatcher> >(m, "Watcher")
    .def(py::init<SharedClient, string>())
    .def("subscribe", [](PyWatcher &a) {
        py::gil_scoped_release gil; // release GIL lock
        return a.watch();
    }, "Start receiving")
    .def("unsubscribe", [](PyWatcher &a) {
        py::gil_scoped_release gil; // release GIL lock
        return a.unwatch();
    }, "Stop watching");

    py::class_<Publisher, std::shared_ptr<Publisher> >(m, "Publisher")
    .def(py::init<SharedClient, string, string>())
    .def("send", [](Publisher &p, uchar* data, int size) {
        py::gil_scoped_release gil; // release GIL lock
        return p.send_message(data, size);
    } , "Send a raw buffer")
    .def("send", [](Publisher &p, MessageWriter& message) {
        py::gil_scoped_release gil; // release GIL lock
        return p.send_message(message);
    }, "Send a writer");

    py::class_<BufferedMessage, std::shared_ptr<Message> >(m, "BufferedMessage")
    .def(py::init<int, uchar*, int, bool>())
    .def(py::init<int, MessageWriter>())
    .def("getChannel", &BufferedMessage::get_channel, "Get channel");

    py::class_<MessageReader>(m, "MessageReader")
    .def("readInt", &MessageReader::read_integer, "Read an integer")
    .def("readLong", &MessageReader::read_long, "Read a long")
    .def("readChar", &MessageReader::read_char, "Read a char")
    .def("readFloat", &MessageReader::read_float, "Read a float")
    .def("readDouble", &MessageReader::read_double, "Read a double")
    .def("readString", &MessageReader::read_string, "Read a string");

    py::class_<MessageWriter>(m, "MessageWriter")
    .def(py::init<ssize_t>(), py::arg("size") = 0)
    .def("writeInt", &MessageWriter::write_integer, "Write an integer")
    .def("writeLong", &MessageWriter::write_long, "Write a long")
    .def("writeChar", &MessageWriter::write_char, "Write a char")
    .def("writeFloat", &MessageWriter::write_float, "Write a float")
    .def("writeDouble", &MessageWriter::write_double, "Write a double")
    .def("writeString", &MessageWriter::write_string, "Write a string");

    return m.ptr();

}
