#!/bin/bash
# 2021-04-22
# Guillaume Le Treut

export CPLUS_INCLUDE_PATH_OLD=$CPLUS_INCLUDE_PATH
export CPLUS_INCLUDE_PATH="$CONDA_PREFIX/include:$CPLUS_INCLUDE_PATH"

export LIBRARY_PATH_OLD=$LIBRARY_PATH
export LIBRARY_PATH="$CONDA_PREFIX/lib:$LIBRARY_PATH"

export LD_LIBRARY_PATH_OLD=$LD_LIBRARY_PATH
export LD_LIBRARY_PATH="$CONDA_PREFIX/lib:$LD_LIBRARY_PATH"

echo CPLUS_INCLUDE_PATH=$CPLUS_INCLUDE_PATH
echo LIBRARY_PATH=$LIBRARY_PATH
echo LD_LIBRARY_PATH=$LD_LIBRARY_PATH
