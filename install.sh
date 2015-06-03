#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

mkdir -p build
{
    cd build
    cmake ..
    make -j $(grep -c processor /proc/cpuinfo) install
}
