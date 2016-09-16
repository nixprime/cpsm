#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

if [[ "${TRAVIS_OS_NAME}" == 'osx' ]]; then
    brew update
    # Skip updating Boost, since doing so takes a long time and we'd like to
    # know about compatibility breakage anyway.
    brew install cmake
    case "${TEST_PY}" in
        py2) brew outdated python || brew upgrade python;;
        py3) brew install python3;;
    esac
fi
