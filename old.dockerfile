ARG UBUNTU_VERSION=16.04

FROM ubuntu:${UBUNTU_VERSION} AS build

LABEL maintainer "luka.cehovin@protonmail.com"

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && \
    apt-get install wget software-properties-common apt-transport-https ca-certificates -y && \
    apt-key adv --keyserver keyserver.ubuntu.com --recv-keys 6AF7F09730B3F0A4 && \
    add-apt-repository ppa:ubuntu-toolchain-r/test -y && apt-add-repository 'deb https://apt.kitware.com/ubuntu/ xenial main' -y

RUN apt-get update && apt-get install -y --no-install-recommends \
    cmake make git pkg-config g++-8 software-properties-common python-pip python-dev python-numpy-dev python-setuptools && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

ENV CC="gcc-8"
ENV CXX="g++-8"

# Install gcc 8
#RUN apt-get update -y && apt-get install gcc-8 g++-8 -y && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 60 --slave /usr/bin/g++ g++ /usr/bin/g++-8 && update-alternatives --config gcc

RUN python --version && cmake --version && gcc-8 --version

# Install pybind

RUN git clone -b v2.8 https://github.com/pybind/pybind11.git && cd pybind11 && mkdir build && cd build && \
cmake -DPYBIND11_TEST=OFF -DPYBIND11_INSTALL=ON .. && make -j install

COPY . /tmp/echolib/

RUN cd /tmp/echolib && mkdir build && cd build && \
    cmake -DBUILD_APPS=OFF -DCMAKE_INSTALL_PREFIX=/tmp/install .. && make -j && make install

RUN export PYTHON_INSTALL=`python -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(prefix='/tmp/install/'))"` && \
    mkdir -p ${PYTHON_INSTALL}/../dist-packages/echolib && cd /tmp/echolib/build/python/ && cp -R echolib ${PYTHON_INSTALL}/../dist-packages/

FROM ubuntu:${UBUNTU_VERSION}

RUN apt-get update && \
    apt-get install wget software-properties-common apt-transport-https ca-certificates -y && \
    add-apt-repository ppa:ubuntu-toolchain-r/test -y && \
    apt-get update && apt-get install -y --no-install-recommends \
    libstdc++6 python python-pip python-numpy python-future python-pyparsing python-jinja2  && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=build /tmp/install /usr/local/
