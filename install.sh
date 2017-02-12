#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

function vim_has {
    local vim="$1"
    local feature="$2"

    # We can't use `vim --version` because neovim is too unique to print
    # +/-python{,3}, so instead we get to play stupid games with script(1).
    local uname="$(uname)"
    case "${uname}" in
        Linux) echo $(script -eqc "${vim} -S <(echo -e \"echo 'x=' . has('${feature}')\\nqa!\")" /dev/null | grep -o 'x=.' | grep -o '[[:digit:]]' -m 1);;
        Darwin | FreeBSD) echo $(script -q /dev/null ${vim} -S <(echo -e "echo 'x=' . has('${feature}')\nqa!") | grep -o 'x=.' | grep -o '[[:digit:]]' -m 1);;
        *) >&2 echo "ERROR: Unknown uname: ${uname}; Vim feature detection not supported"; false;;
    esac
}

if [ -z "${PY3+x}" ]; then
    VIM="${VIM:-vim}"
    echo "PY3 not specified; inferring Python version from ${VIM}"
    have_py2=$(vim_has ${VIM} python)
    have_py3=$(vim_has ${VIM} python3)
    if [ "${have_py3}" -eq "1" ]; then
        echo "Python 3 selected"
        PY3="ON"
    elif [ "${have_py2}" -eq "1" ]; then
        echo "Python 2 selected"
        PY3="OFF"
    else
        >&2 echo "ERROR: No Python support detected"
        false
    fi
else
    case "${PY3}" in
        ON) echo "Python 3 selected by PY3=${PY3}";;
        OFF) echo "Python 2 selected by PY3=${PY3}";;
        *) >&2 echo "ERROR: invalid PY3=${PY3}"; false;;
    esac
fi

cd "$(dirname "${BASH_SOURCE[0]}")"
rm -rf bin/* build/*
mkdir -p bin build
(
    cd build
    cmake -DPY3:BOOL=${PY3} ..
    make install && make test
)
