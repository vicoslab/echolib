/* -*- Mode: C++; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */

#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

#include <echolib/datatypes.h>
#include <echolib/client.h>

using namespace echolib;
namespace py = pybind11;

typedef function<void(string)> MessageCallback;

class PySubscriber : public Subscriber, public std::enable_shared_from_this<PySubscriber> {
  public:
    PySubscriber(SharedClient client, const string &alias, const string &type, function<void(SharedMessage)> callback) : Subscriber(client, alias, type), callback(callback) {

    }

    virtual void on_message(SharedMessage message) {
        py::gil_scoped_acquire gil; // acquire GIL lock
        callback(message);
    }

    using Subscriber::subscribe;
    using Subscriber::unsubscribe;

  private:

    function<void(SharedMessage)> callback;

};


namespace pybind11 {
namespace detail {

#if PY_VERSION_HEX < 0x03000000
#define MyPyText_AsString PyString_AsString
#else
#define MyPyText_AsString PyUnicode_AsUTF8
#endif


template <> class type_caster<SharedDictionary> {
    typedef SharedDictionary type;
public:
    bool load(py::handle src, bool) {
        py::gil_scoped_acquire gil;
        if (!src || src.ptr() == Py_None || !PyDict_Check(src.ptr())) { return false; }
        Py_ssize_t ppos = 0;
        PyObject *pkey, *pvalue;
        value = make_shared<Dictionary>();
        while (PyDict_Next(src.ptr(), &ppos, &pkey, &pvalue))
            value->set(MyPyText_AsString(pkey), MyPyText_AsString(pvalue));
        return true;
    }
    static py::handle cast(const SharedDictionary &src, return_value_policy policy, py::handle parent) {
        py::gil_scoped_acquire gil;
        py::dict pydata;
        for (DictionaryIterator it = src->begin(); it != src->end(); it++) {
            pydata[py::str(it->first)] = py::str(it->second);
        }
        return py::handle(pydata);
    }
    PYBIND11_TYPE_CASTER(SharedDictionary, _("echolib::SharedDictionary"));
};

}
}

class PyIOBaseObserver : public IOBaseObserver {
  public:
    using IOBaseObserver::IOBaseObserver;

    PyIOBaseObserver() {

    }

    ~PyIOBaseObserver() {

    }

    virtual void on_output(SharedIOBase client) override {
        PYBIND11_OVERLOAD_PURE(
            void,
            IOBaseObserver,
            on_output,
            client
        );
    }
};


class PyWatcher : public Watcher {
  public:
    using Watcher::Watcher;

    void on_event(SharedDictionary command) override {
        PYBIND11_OVERLOAD_PURE(
            void,
            Watcher,
            on_event,
            command
        );
    }
};

class PyIOBase : public IOBase {
public:
    using IOBase::IOBase;

    virtual int get_file_descriptor() {
        PYBIND11_OVERLOAD_PURE(int, IOBase, fd, );
    }

    virtual bool handle_input() {
        PYBIND11_OVERLOAD_PURE(bool, IOBase, handle_input, );
    }

    virtual bool handle_output() {
        PYBIND11_OVERLOAD_PURE(bool, IOBase, handle_output, );
    }

    virtual void disconnect() {
        PYBIND11_OVERLOAD_PURE(void, IOBase, disconnect, );
    }



};

void write_timestamp(MessageWriter& writer, const std::chrono::system_clock::time_point& src) {

    write(writer, src);

}

std::chrono::system_clock::time_point read_timestamp(MessageReader& reader) {

    std::chrono::system_clock::time_point res;

    read(reader, res);

    return res;

}

PYBIND11_MODULE(pyecho, m) {

    //py::module m("pyecho", "Echo IPC library Python bindings");
    m.doc() = "Echo IPC library Python bindings";

    py::class_<IOBase, PyIOBase, std::shared_ptr<IOBase> >(m, "IOBase")
    .def(py::init())
    .def("handle_input", &IOBase::handle_input, "Handle input messages")
    .def("handle_output", &IOBase::handle_output, "Handle output messages")
    .def("fd", &IOBase::get_file_descriptor, "Get access to low-level file descriptor")
    .def("disconnect", &IOBase::disconnect, "Disconnect the client")
    .def("observe", &IOBase::observe, "Observe client for changes")
    .def("unobserve", &IOBase::unobserve, "Stop observing client for changes");

    py::class_<IOLoop, std::shared_ptr<IOLoop> >(m, "IOLoop")
    .def(py::init())
    .def("add_handler", &IOLoop::add_handler, "Register handler")
    .def("remove_handler", &IOLoop::remove_handler, "Unregister handler")
    .def("wait", [](IOLoop& c, long timeout) {
        py::gil_scoped_release gil; // release GIL lock
        return c.wait(timeout);
    }, "Wait for more messages");

    py::class_<Client, IOBase, std::shared_ptr<Client> >(m, "Client")
    .def(py::init<const string&, const string&>(), py::arg("name") = string(""), py::arg("address") = string(""))
    .def("disconnect", &Client::disconnect, "Disconnect the client")
    .def("handle_input", &Client::handle_input, "Handle input messages")
    .def("handle_output", &Client::handle_output, "Handle output messages")
    .def("fd", &Client::get_file_descriptor, "Get access to low-level file descriptor")
    .def("isConnected", &Client::is_connected, "Check if the client is connected");

    py::class_<Subscriber, PySubscriber, std::shared_ptr<Subscriber> >(m, "Subscriber")
    .def(py::init<SharedClient, string, string, function<void(SharedMessage)> >())
    .def("subscribe", [](PySubscriber &a) {
        py::gil_scoped_release gil; // release GIL lock
        return a.subscribe();
    }, "Start receiving")
    .def("unsubscribe", [](PySubscriber &a) {
        py::gil_scoped_release gil; // release GIL lock
        return a.unsubscribe();
    }, "Stop receiving");

    py::class_<Watcher, PyWatcher, std::shared_ptr<Watcher> >(m, "Watcher")
    .def(py::init<SharedClient, string>())
    .def("watch", [](PyWatcher &a) {
        py::gil_scoped_release gil; // release GIL lock
        return a.watch();
    }, "Start watching")
    .def("unwatch", [](PyWatcher &a) {
        py::gil_scoped_release gil; // release GIL lock
        return a.unwatch();
    }, "Stop watching");

    py::class_<IOBaseObserver, PyIOBaseObserver, std::shared_ptr<IOBaseObserver> >(m, "IOBaseObserver")
    .def(py::init());

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

    py::class_<MemoryBuffer, std::shared_ptr<MemoryBuffer> >(m, "MemoryBuffer")
    .def("size", &MemoryBuffer::get_length, "Get message length");

    py::class_<Message, std::shared_ptr<Message> >(m, "Message")
    .def("getChannel", &Message::get_channel, "Get channel");

    py::class_<BufferedMessage, Message, MemoryBuffer, std::shared_ptr<BufferedMessage> >(m, "BufferedMessage")
    .def(py::init<uchar*, size_t, bool>())
    .def("getChannel", &BufferedMessage::get_channel, "Get channel");

    py::class_<MessageReader>(m, "MessageReader")
    .def(py::init<SharedMessage>())
    .def("readShort", &MessageReader::read_short, "Read a short")
    .def("readInt", &MessageReader::read_integer, "Read an integer")
    .def("readBool", &MessageReader::read_bool, "Read a boolean")
    .def("readLong", &MessageReader::read_long, "Read a long")
    .def("readChar", &MessageReader::read_char, "Read a char")
    .def("readFloat", &MessageReader::read_float, "Read a float")
    .def("readDouble", &MessageReader::read_double, "Read a double")
    .def("readString", &MessageReader::read_string, "Read a string");

    py::class_<MessageWriter>(m, "MessageWriter")
    .def(py::init<size_t>(), py::arg("size") = (size_t) 0)
    .def("writeShort", &MessageWriter::write_short, "Write a short")
    .def("writeInt", &MessageWriter::write_integer, "Write an integer")
    .def("writeBool", &MessageWriter::write_bool, "Write a boolean")
    .def("writeLong", &MessageWriter::write_long, "Write a long")
    .def("writeChar", &MessageWriter::write_char, "Write a char")
    .def("writeFloat", &MessageWriter::write_float, "Write a float")
    .def("writeDouble", &MessageWriter::write_double, "Write a double")
    .def("writeString", &MessageWriter::write_string, "Write a string")
    .def("cloneData", &MessageWriter::clone_data, "Clone current data to message");
/*
    py::class_<Dictionary, std::shared_ptr<Dictionary> >(m, "Dictionary")
    .def(py::init<Dictionary>())
    .def("getShort", &Dictionary::get<short>, "Read a short");
*/

    m.def("readTimestamp", &read_timestamp, "Read a timestamp from message");
    m.def("writeTimestamp", &write_timestamp, "Write a timestamp to message");

}
