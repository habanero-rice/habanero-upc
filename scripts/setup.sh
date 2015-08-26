#!/bin/bash

export UPCPP_ROOT=/Users/vivek/rice_work/projects/upcxx-repos/upcxx-install
export HCPP_BASE=/Users/vivek/rice_work/projects/release/hcpp
export CXX=mpicxx
export CC=gcc
export LIBXML2=/usr/local/Cellar/libxml2/2.9.2
#export TBB_MALLOC=/Users/vivek/rice_work/projects/tbb/mac

################################################
#
# DO NOT MODIFY ANYTHING BELOW UNLESS YOU ARE
# CHANGING THE INSTALLATION PATH OF HABANERO-UPC
#
################################################

cd ..
BASE=`pwd`
cd -

export HCPP_ROOT=${HCPP_BASE}/hcpp-install
export HABANERO_UPC_ROOT=${BASE}/habanero-upc-install
export LD_LIBRARY_PATH=${HABANERO_UPC_ROOT}/lib:${HCPP_ROOT}/lib:$LD_LIBRARY_PATH

if [ ! -z "${TBB_MALLOC}" ]; then
   export LD_LIBRARY_PATH=${TBB_MALLOC}:$LD_LIBRARY_PATH
fi
