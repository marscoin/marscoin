#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SRC_DIR="$ROOT_DIR/liboqs"
BUILD_DIR="$ROOT_DIR/build"
INSTALL_DIR="$BUILD_DIR/install"
PARALLEL="${OQS_BUILD_PARALLEL:-1}"

if [[ ! -f "$SRC_DIR/CMakeLists.txt" ]]; then
  echo "liboqs source snapshot missing at: $SRC_DIR" >&2
  exit 1
fi

mkdir -p "$BUILD_DIR"

rm -rf "$BUILD_DIR/CMakeFiles" "$BUILD_DIR/src" "$INSTALL_DIR"

cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release \
  -DOQS_DIST_BUILD=OFF \
  -DBUILD_SHARED_LIBS=OFF \
  -DOQS_BUILD_ONLY_LIB=ON \
  -DOQS_MINIMAL_BUILD="SIG_sphincs_sha2_128s_simple" \
  -DOQS_USE_OPENSSL=OFF \
  -DOQS_ENABLE_KEM_BIKE=OFF \
  -DOQS_ENABLE_KEM_CLASSIC_MCELIECE=OFF \
  -DOQS_ENABLE_SIG_STFL_XMSS=OFF \
  -DOQS_ENABLE_SIG_STFL_LMS=OFF \
  -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR"

cmake --build "$BUILD_DIR" --target oqs --parallel "$PARALLEL"
cmake --install "$BUILD_DIR"

echo "Built vendored liboqs artifacts at: $INSTALL_DIR"
