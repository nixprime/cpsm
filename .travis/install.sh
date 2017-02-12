#!/bin/bash

set -o errexit
set -o nounset
set -o pipefail

if [[ "${TRAVIS_OS_NAME}" == 'osx' ]]; then
    brew update
    # Skip updating Boost, since doing so takes a long time and we'd like to
    # know about compatibility breakage anyway.
    brew install cmake || brew outdated cmake || brew upgrade cmake
    case "${TEST_PY}" in
        py2) brew install python || brew outdated python || brew upgrade python;;
        py3) brew install python3 || brew outdated python3 || brew upgrade python3;;
    esac
fi
