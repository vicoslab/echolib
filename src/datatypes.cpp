

#include <echolib/datatypes.h>

namespace echolib {

Header::Header(std::string source, std::chrono::system_clock::time_point timestamp): source(source), timestamp(timestamp) {

}

Header::~Header() {

}

}