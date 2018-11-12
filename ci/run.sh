#!/usr/bin/env sh

# Configure
mkdir build
cd build
cmake ..

# Build and run tests
make check -j 4
