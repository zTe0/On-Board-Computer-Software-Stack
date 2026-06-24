#!/usr/bin/env bash
# =============================================================
#  build.sh — Configure and build obcsw
#  Usage:
#    ./build.sh             → Release build
#    ./build.sh debug       → Debug build (ASan + UBSan)
#    ./build.sh test        → Build then run tests
#    ./build.sh clean       → Remove build directory
# =============================================================

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_TYPE="${1:-release}"

case "${BUILD_TYPE,,}" in
    clean)
        echo "[build.sh] Removing build directories..."
        rm -rf "${SCRIPT_DIR}/build"
        echo "[build.sh] Done."
        exit 0
        ;;
    debug)
        CMAKE_BUILD_TYPE="Debug"
        BUILD_DIR="${SCRIPT_DIR}/build/debug"
        ;;
    test)
        CMAKE_BUILD_TYPE="Release"
        BUILD_DIR="${SCRIPT_DIR}/build/release"
        RUN_TESTS=1
        ;;
    *)
        CMAKE_BUILD_TYPE="Release"
        BUILD_DIR="${SCRIPT_DIR}/build/release"
        RUN_TESTS=0
        ;;
esac

echo "[build.sh] Build type : ${CMAKE_BUILD_TYPE}"
echo "[build.sh] Build dir  : ${BUILD_DIR}"

mkdir -p "${BUILD_DIR}"

cmake \
    -S "${SCRIPT_DIR}" \
    -B "${BUILD_DIR}" \
    -DCMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}" \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

cmake --build "${BUILD_DIR}" --parallel "$(nproc)"

echo ""
echo "[build.sh] Build complete."
echo "[build.sh] Binary: ${BUILD_DIR}/obcsw"

if [[ "${RUN_TESTS:-0}" == "1" ]]; then
    echo ""
    echo "[build.sh] Running tests..."
    cd "${BUILD_DIR}" && ctest --output-on-failure
fi
