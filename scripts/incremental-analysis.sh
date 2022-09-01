#!/bin/bash

mkdir -p build
pushd build
cmake .. -DVERTEX_COALESCING=Off -DOPT_WR_WINDOW=1
make -j main
popd

HP=" " DB=" " RESULT_FILE="baseline" ATTR="baseline" ./scripts/driver.sh

pushd build
cmake .. -DVERTEX_COALESCING=Off -DOPT_WR_WINDOW=15
make -j main
popd
HP=" " DB=" " RESULT_FILE="baseline_WRBatching" ATTR="WRBatching" ./scripts/driver.sh

pushd build
cmake .. -DVERTEX_COALESCING=On -DOPT_WR_WINDOW=15
make -j main
popd
HP=" " DB=" " RESULT_FILE="baseline_WRBatching_Coalesce" ATTR="WRBatching_Coalescing" ./scripts/driver.sh

pushd build
cmake .. -DVERTEX_COALESCING=On -DOPT_WR_WINDOW=15
make -j main
popd
HP="--hp" DB=" " RESULT_FILE="baseline_WRBatching_Coalesce_HP" ATTR="WRBatching_Coalescing_HP" ./scripts/driver.sh

pushd build
cmake .. -DVERTEX_COALESCING=On -DOPT_WR_WINDOW=8
make -j main
popd
HP="--hp" DB="--double-buffer" RESULT_FILE="baseline_WRBatching_Coalesce_HP_DB" ATTR="WRBatching_Coalescing_HP_DB" ./scripts/driver.sh
