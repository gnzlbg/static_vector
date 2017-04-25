#!/usr/bin/env bash

TRUNK_VERSION="5.0.0"

set -e

echo "Fetching libc++ and libc++abi tip-of-trunk..."

# Checkout LLVM sources
git clone --depth=1 https://github.com/llvm-mirror/llvm.git llvm-source
git clone --depth=1 https://github.com/llvm-mirror/libcxx.git llvm-source/projects/libcxx
git clone --depth=1 https://github.com/llvm-mirror/libcxxabi.git llvm-source/projects/libcxxabi

mkdir llvm-build
cd llvm-build
cmake -DCMAKE_C_COMPILER=${C_COMPILER} -DCMAKE_CXX_COMPILER=${CXX} \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr \
      -DLIBCXX_ABI_UNSTABLE=ON \
      ../llvm-source

make cxx -j2 VERBOSE=1
sudo make install-cxxabi install-cxx
