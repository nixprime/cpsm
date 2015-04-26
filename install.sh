#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

python setup.py build
mkdir -p python/
cp build/lib*/* python/
