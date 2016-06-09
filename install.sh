#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

if vim --version | grep -q +python3; then
    PY3="ON"
else
    PY3="OFF"
fi

mkdir -p build
{
    cd build
    cmake -DPY3:BOOL=$PY3 ..
    make install
}
