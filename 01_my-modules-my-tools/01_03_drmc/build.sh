#!/bin/bash
# build-drm.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export TOOLCHAIN_ROOT="$HOME/x-tools/arm-unknown-linux-gnueabihf"
export PATH="$TOOLCHAIN_ROOT/bin:$PATH"

export CC=arm-unknown-linux-gnueabihf-gcc
export CXX=arm-unknown-linux-gnueabihf-g++
export AR=arm-unknown-linux-gnueabihf-ar
export RANLIB=arm-unknown-linux-gnueabihf-ranlib
export STRIP=arm-unknown-linux-gnueabihf-strip

export CFLAGS="-static -O2 -pipe -Wall -Wextra"
export LDFLAGS="-static"

echo "Building drm for ARM..."
$CC $CFLAGS -I"$TOOLCHAIN_ROOT/include" \
    -o drm drm.c \
    $LDFLAGS -L"$TOOLCHAIN_ROOT/lib" \
    -lssl -lcrypto -lzip -lxml2 -lz -lm

echo "Stripping binary..."
$STRIP drm

echo "Done! Binary size:"
ls -lh drm

echo ""
echo "To install on Kobo:"
echo "  scp drm kobo:/mnt/onboard/.adds/bin/"
