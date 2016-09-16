#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

case "${TEST_PY}" in
    py2) PY3=OFF ./install.sh;;
    py3) PY3=ON ./install.sh;;
    *) echo "Unknown TEST_PY: ${TEST_PY}"; false;;
esac
