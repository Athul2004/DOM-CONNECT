#!/bin/bash
set -e

echo "Cross-compiling for aarch64 (Orange Pi Zero 3)..."

mkdir -p build-arm
cd build-arm

# Note: Requires gcc-aarch64-linux-gnu installed on host
cmake .. -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc -DCMAKE_BUILD_TYPE=Release

make -j$(nproc)

echo "Build complete: build-arm/zigbee-gateway"
