# Echo IPC library #

A simple and efficient interprocess communication (IPC) library for Linux written in C++11. The core part of the code was developed as a bachelor's thesis of Urban Škvorc with the title `Development of a library for interprocess communication in interactive systems` in 2015 at the University of Ljubljana, Faculty of Computer and Information Science. The library was later added some documentation and some polish and is now made available under MIT license.

## Contributors ##

 * Luka Čehovin Zajc (developer, maintainer)
 * Rok Mandeljc (contributor)
 * Urban Škvorc (initial developer)

## Installation ##

The C++ library does not have any dependencies apart from a C++11 capable compiler, but if you want to compile Python bindings you have to download the [pybind11](https://github.com/pybind/pybind11) template library. To use Python wrapper, numpy is required. Some other Python packages are required for message file generation. Optional support is provided for OpenCV images and camera access.

