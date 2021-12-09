ARG UBUNTU_VERSION=18.04

FROM ubuntu:${UBUNTU_VERSION} AS build

LABEL maintainer "luka.cehovin@protonmail.com"

ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake make git pkg-config g++ software-properties-common python3-dev python3-numpy-dev python3-setuptools && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

WORKDIR /tmp

RUN git clone https://github.com/pybind/pybind11.git && cd pybind11 && mkdir build && cd build && cmake -DPYBIND11_TEST=OFF -DPYBIND11_INSTALL=ON .. && make -j install

COPY . /tmp/echolib/

RUN cd /tmp/echolib && mkdir build && cd build && \
    cmake -DBUILD_APPS=OFF -DCMAKE_INSTALL_PREFIX=/tmp/install .. && make -j && make install

RUN export PYTHON_INSTALL=`python3 -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(prefix='/tmp/install/'))"` && \
    mkdir -p ${PYTHON_INSTALL}/../dist-packages/echolib && cd /tmp/echolib/build/python/ && cp -R echolib ${PYTHON_INSTALL}/../dist-packages/

FROM ubuntu:${UBUNTU_VERSION}

RUN apt-get update && apt-get install -y --no-install-recommends \
    python3 python3-pip python3-numpy && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY --from=build /tmp/install /usr/local/
