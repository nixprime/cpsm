#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail
set +o histexpand

if [ -z ${PY3+x} ]; then
    # neovim's --version doesn't indicate the presence or absence of python3
    # support.
    have_py3=$(script -q --return -c "vim -S <(echo -e \"redir! > /dev/stderr\\necho has('python3')\\nqa!\") 2>&3" /dev/null 3>&1 >/dev/null)
    if [ "$have_py3" -eq "1" ]; then
        PY3="ON"
    else
        PY3="OFF"
    fi
fi
echo "PY3=$PY3"

rm -rf build
mkdir -p build
{
    cd build
    cmake -DPY3:BOOL=$PY3 ..
    make install
}
