#!/bin/bash

mkdir -p build

# pushd build
# cmake .. -DVERTEX_COALESCING=On -DOPT_WR_WINDOW=30 -DCMAKE_CXX_COMPILER=dpcpp
# make -j main
# popd
# HP="--hp" DB="" RESULT_FILE="best_single_buffer_dpcpp" ATTR="WRBatching_Coalescing_HP_dpcpp" ./scripts/driver.sh

# pushd build
# cmake .. -DVERTEX_COALESCING=On -DOPT_WR_WINDOW=15 -DCMAKE_CXX_COMPILER=dpcpp
# make -j main
# popd
# HP="--hp" DB="--double-buffer" RESULT_FILE="best_double_buffer_dpcpp" ATTR="WRBatching_Coalescing_HP_DB_dpcpp" ./scripts/driver.sh

pushd build
cmake .. -DVERTEX_COALESCING=On -DOPT_WR_WINDOW=5 -DCMAKE_CXX_COMPILER=dpcpp
make -j main
popd
HP="--hp" DB="--double-buffer" RESULT_FILE="bfs_rerun" ATTR="WRBatching_Coalescing_HP_dpcpp" ./scripts/driver.sh
