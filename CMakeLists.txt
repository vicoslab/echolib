cmake_minimum_required(VERSION 3.1)

project(echo CXX)
set(PROJECT_VERSION 0.1.0)

include(GNUInstallDirs)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

option(BUILD_APPS "Build echolib apps" ON)
option(BUILD_PYTHON "Build python wrapper" ON)
option(BUILD_EXAMPLES "Build examples" OFF)
option(BUILD_DEBUG "Enable debug output" OFF)
option(BUILD_TESTS "Build tests" OFF)

find_package(OpenCV QUIET COMPONENTS core videoio highgui)

if (OpenCV_FOUND)
    set(BUILD_OPENCV ON)
    include_directories(${OpenCV_INCLUDE_DIR})
endif()

if (CMAKE_VERSION VERSION_LESS "3.1")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
    endif ()
else ()
    set (CMAKE_CXX_STANDARD 17)
endif ()

if(CMAKE_COMPILER_IS_GNUCXX)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -pedantic")
    set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -flto")
endif()

if(BUILD_DEBUG)
	add_definitions(-DECHOLIB_DEBUG)
    add_definitions(-DECHOLIB_SOURCE_COMPILE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/src/")
endif()

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/ ${CMAKE_CURRENT_SOURCE_DIR}/include/)

set(ECHO_SRC 
    src/array.cpp
    src/loop.cpp
    src/message.cpp
    src/client.cpp
    src/server.cpp
    src/routing.cpp
    src/datatypes.cpp
    src/debug.cpp
)

set(ECHO_HEADERS 
    include/echolib/loop.h
    include/echolib/client.h
    include/echolib/camera.h
    include/echolib/server.h
    include/echolib/routing.h
    include/echolib/message.h
    include/echolib/datatypes.h
    include/echolib/helpers.h
    include/echolib/array.h
)

# echo shared library
add_library(echo SHARED ${ECHO_SRC})

target_compile_options(echo PUBLIC "-pthread")
target_link_libraries(echo PUBLIC "pthread")

if (BUILD_OPENCV)
target_compile_definitions(echo PUBLIC "BUILD_OPENCV")
TARGET_LINK_LIBRARIES(echo PUBLIC ${OpenCV_LIBS})
endif()

#target_compile_options(echo_static PUBLIC "-pthread" "-fPIC")
#target_link_libraries(echo_static PUBLIC "pthread")

set_target_properties(echo PROPERTIES VERSION 0.0.0 SOVERSION 0)

install(TARGETS echo EXPORT echolib_targets DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${ECHO_HEADERS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/echolib)

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
if(BUILD_APPS)
    add_executable(echorouter src/apps/router.cpp)
    target_link_libraries(echorouter echo)
    install(TARGETS echorouter DESTINATION ${CMAKE_INSTALL_BINDIR})

    if (BUILD_OPENCV)
        ADD_EXECUTABLE(echo_camera src/apps/cameraserver.cpp)
        TARGET_LINK_LIBRARIES(echo_camera echo ${OpenCV_LIBS})
        ADD_EXECUTABLE(echo_image src/apps/imageserver.cpp)
        TARGET_LINK_LIBRARIES(echo_image echo ${OpenCV_LIBS})
        ADD_EXECUTABLE(echo_video src/apps/videoserver.cpp)
        TARGET_LINK_LIBRARIES(echo_video echo ${OpenCV_LIBS})
        ADD_EXECUTABLE(echo_client src/apps/videoclient.cpp)
        TARGET_LINK_LIBRARIES(echo_client echo ${OpenCV_LIBS})
        install(TARGETS echo_camera echo_image echo_video echo_client DESTINATION ${CMAKE_INSTALL_BINDIR})
        
    endif()
endif()

# Python bindings
if(BUILD_PYTHON)
    option(BUILD_PYTHON_SOURCE "Build python source package" OFF)

    ADD_CUSTOM_TARGET(copy_library_python ALL)

    SET(BUILD_PYTHON_DIR "${CMAKE_BINARY_DIR}/python")
    FILE(MAKE_DIRECTORY ${BUILD_PYTHON_DIR}/echolib)

    IF(BUILD_PYTHON_SOURCE)

        FILE(MAKE_DIRECTORY ${BUILD_PYTHON_DIR}/echolib/echolib)

        ADD_CUSTOM_COMMAND(TARGET copy_library_python COMMAND 
            ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} 
            ${CMAKE_COMMAND} -E copy ${ECHO_SRC} src/python/wrapper.cpp src/algorithms.h src/debug.h
            ${BUILD_PYTHON_DIR}/echolib/echolib)
        ADD_CUSTOM_COMMAND(TARGET copy_library_python COMMAND ${CMAKE_COMMAND} -E chdir ${CMAKE_SOURCE_DIR} ${CMAKE_COMMAND} -E copy ${ECHO_HEADERS} ${BUILD_PYTHON_DIR}/echolib/echolib)

    ELSE()

        find_path(PYBIND11_INCLUDE_DIR pybind11/pybind11.h HINTS /usr/include DOC "PyBind11 include directory")
        find_package(Python REQUIRED COMPONENTS Interpreter Development NumPy)

        add_library(pyecho SHARED src/python/wrapper.cpp ${ECHO_SRC})
        set_target_properties(pyecho PROPERTIES LIBRARY_OUTPUT_DIRECTORY ${BUILD_PYTHON_DIR}/echolib)
        set_target_properties(pyecho PROPERTIES PREFIX "")
        target_include_directories(pyecho PRIVATE ${Python_INCLUDE_DIRS} ${PYBIND11_INCLUDE_DIR})


        install(CODE "execute_process(COMMAND whoami)")
        install(CODE "execute_process(COMMAND ${Python_EXECUTABLE} setup.py install WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/python)")

    ENDIF()


    SET(SOURCES_PY 
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/__init__.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/__main__.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/ignition.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/tornado.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/_image.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/messages/__main__.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/messages/library/__init__.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/messages/library/geometry.msg
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/messages/templates/__init__.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/messages/templates/cpp.tpl
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/messages/templates/python.tpl
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/messages/__init__.py
        ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib/messages/cli.py
    )


    ADD_CUSTOM_COMMAND(TARGET copy_library_python COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/python/setup.py ${BUILD_PYTHON_DIR}/)
    ADD_CUSTOM_COMMAND(TARGET copy_library_python COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/python/echolib ${BUILD_PYTHON_DIR}/echolib)


endif()

# Examples
if(BUILD_EXAMPLES)
    add_executable(chat src/examples/chat.cpp)
    target_link_libraries(chat echo)

    add_executable(chunked src/examples/chunked.cpp)
    target_link_libraries(chunked echo)
endif()

# Examples
if(BUILD_TESTS)
    add_executable(deadend src/tests/deadend.cpp)
    target_link_libraries(deadend echo)

    add_executable(dynamic_client src/tests/dynamic_client.cpp)
    target_link_libraries(dynamic_client echo)

    add_executable(test_tensor src/tests/tensor.cpp)
    target_link_libraries(test_tensor echo)

endif()
