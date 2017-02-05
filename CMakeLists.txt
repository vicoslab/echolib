cmake_minimum_required(VERSION 2.8.11)

project(echo CXX)
set(PROJECT_VERSION 0.1.0)

include(GNUInstallDirs)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(BUILD_DAEMON "Build echolib daemon" ON)
option(BUILD_PYTHON "Build python bindings" ON)
option(BUILD_EXAMPLES "Build examples" OFF)
option(BUILD_DEBUG "Enable debug output" OFF)

set(ECHO_PYTHON_VERSION "2" CACHE STRING "Python version to use for compiling python bindings")

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall -pedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")
endif()

if(BUILD_DEBUG)
	add_definitions(-DECHOLIB_DEBUG)
    add_definitions(-DECHOLIB_SOURCE_COMPILE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/src/")
endif()

# echo shared library
add_library(echo SHARED
    src/loop.cpp
    src/message.cpp
    src/client.cpp
    src/server.cpp
    src/routing.cpp    
    src/debug.cpp
)

target_compile_options(echo PUBLIC "-pthread")
target_link_libraries(echo PUBLIC "pthread")

set_target_properties(echo PROPERTIES
    VERSION 0.0.0
    SOVERSION 0
)

install(TARGETS echo EXPORT echolib_targets DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES
    src/loop.h
    src/client.h
    src/server.h
    src/routing.h    
    src/message.h
    src/datatypes.h
    src/helpers.h
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/echolib
)

# CMake config file
include(CMakePackageConfigHelpers)

set(LIB_INSTALL_DIR ${CMAKE_INSTALL_LIBDIR})
set(INCLUDE_INSTALL_DIR ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(echolib-config.cmake.in
    ${PROJECT_BINARY_DIR}/echolib-config.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/echolib
    PATH_VARS LIB_INSTALL_DIR INCLUDE_INSTALL_DIR)

write_basic_package_version_file(
    ${PROJECT_BINARY_DIR}/echolib-config-version.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/echolib-config.cmake
    ${CMAKE_CURRENT_BINARY_DIR}/echolib-config-version.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/echolib)

install(
    EXPORT echolib_targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/echolib
    FILE echolib-targets.cmake
)

# echo daemon
if(BUILD_DAEMON)
    add_executable(echodaemon
        src/daemon.cpp
    )

    target_link_libraries(echodaemon echo)

    install(TARGETS echodaemon DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# Python bindings
if(BUILD_PYTHON)
    find_path(PYBIND11_INCLUDE_DIR pybind11.h HINTS /usr/include PATH_SUFFIXES pybind11 DOC "PyBind11 include directory")
    find_package(PythonInterp ${ECHOCV_PYTHON_VERSION} REQUIRED)
    find_package(PythonLibs ${ECHOCV_PYTHON_VERSION} REQUIRED)

    add_library(pyecho SHARED src/python.cpp)
    target_link_libraries(pyecho echo ${PYTHON_LIBRARY})
    set_target_properties(pyecho PROPERTIES PREFIX "")
    target_include_directories(pyecho PRIVATE ${PYTHON_INCLUDE_DIRS} ${PYBIND11_INCLUDE_DIR})

    # Determine installation path
    execute_process(COMMAND
        ${PYTHON_EXECUTABLE} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(prefix='${CMAKE_INSTALL_PREFIX}'))"
        OUTPUT_VARIABLE PYTHON_SITE_PACKAGES OUTPUT_STRIP_TRAILING_WHITESPACE)

    install(TARGETS pyecho DESTINATION ${PYTHON_SITE_PACKAGES}/echolib)
    install(DIRECTORY python/echolib DESTINATION ${PYTHON_SITE_PACKAGES})
endif()

# Examples
if(BUILD_EXAMPLES)
    add_executable(chat src/examples/chat.cpp)
    target_include_directories(chat PRIVATE src)
    target_link_libraries(chat echo)

    add_executable(chunked src/examples/chunked.cpp)
    target_include_directories(chunked PRIVATE src)
    target_link_libraries(chunked echo)
endif()


