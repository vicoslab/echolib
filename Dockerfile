ARG UBUNTU_VERSION=16.04

FROM ubuntu:${UBUNTU_VERSION} AS build

LABEL maintainer "luka.cehovin@protonmail.com"

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake make git pkg-config g++ software-properties-common python-pip python-dev python-numpy-dev python-setuptools && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

RUN python --version
RUN cmake --version
RUN gcc --version

# Install gcc 8

RUN apt-get update && apt-get install wget build-essential software-properties-common -y
RUN add-apt-repository ppa:ubuntu-toolchain-r/test -y
RUN apt-get update -y && apt-get install gcc-8 g++-8 -y && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 60 --slave /usr/bin/g++ g++ /usr/bin/g++-8 && update-alternatives --config gcc

# Update cmake

RUN apt install libssl-dev -y
RUN apt remove cmake -y
RUN wget https://github.com/Kitware/CMake/releases/download/v3.23.0-rc5/cmake-3.23.0-rc5.tar.gz
RUN tar -xf cmake-3.23.0-rc5.tar.gz && cd cmake-3.23.0-rc5 && ./bootstrap && make -j5 && make install

# Install pybind

RUN git clone -b v2.8 https://github.com/pybind/pybind11.git && cd pybind11 && mkdir build && cd build && \
cmake -DPYBIND11_TEST=OFF -DPYBIND11_INSTALL=ON .. && make -j install

COPY . /tmp/echolib/

RUN cd /tmp/echolib && mkdir build && cd build && \
    cmake -DBUILD_APPS=OFF -DCMAKE_INSTALL_PREFIX=/tmp/install .. && make -j && make install

RUN export PYTHON_INSTALL=`python -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(prefix='/tmp/install/'))"` && \
    mkdir -p ${PYTHON_INSTALL}/../dist-packages/echolib && cd /tmp/echolib/build/python/ && cp -R echolib ${PYTHON_INSTALL}/../dist-packages/

FROM ubuntu:${UBUNTU_VERSION}

RUN apt-get update && apt-get install -y --no-install-recommends \
    python python-pip python-numpy && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=build /tmp/install /usr/local/
