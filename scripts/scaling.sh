#!/bin/bash

mkdir -p build
pushd build
cmake .. -DCMAKE_CXX_COMPILER=dpcpp
make -j main
popd

# NUMA="--no-numa-bind"

# 10 20 30 40
for T in 10; do
    THREADS="$T" RESULT_FILE="prnobindT${T}" ATTR="scaling" ./scripts/driver.sh
done
