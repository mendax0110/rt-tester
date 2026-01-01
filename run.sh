#!/bin/bash
mkdir -p build
cd build || exit 1
cmake .. || exit 1
cmake --build . || exit 1
cd rt-tester || exit 1
chmod u+x ./rt-tester || exit 1
echo "Build and setup complete. You can now run ./rt-tester."