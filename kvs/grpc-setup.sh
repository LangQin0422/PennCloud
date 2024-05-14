#!/bin/bash

sudo apt-get install build-essential
sudo apt-get update 
sudo apt-get install -y build-essential autoconf libtool pkg-config cmake git
sudo apt-get install xterm
sudo apt-get install libssl-dev

cd ~
git clone --recurse-submodules -b v1.62.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc

cd grpc
mkdir -p cmake/build
pushd cmake/build
cmake -DgRPC_INSTALL=ON \
      -DgRPC_BUILD_TESTS=OFF \
      -DCMAKE_INSTALL_PREFIX=$MY_INSTALL_DIR \
      ../..

make -j$(nproc) install