#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CMAKE_GENERATOR="${CMAKE_GENERATOR:-Ninja}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
BUILD_DIR="${BUILD_DIR:-${SCRIPT_DIR}/build_linux_${BUILD_TYPE}}"

cmake -S "${SCRIPT_DIR}" -B "${BUILD_DIR}" -G "${CMAKE_GENERATOR}" -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
cmake --build "${BUILD_DIR}" --config "${BUILD_TYPE}"
