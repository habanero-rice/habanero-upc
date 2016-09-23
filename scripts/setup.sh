#!/bin/bash

########################################
##### SET FOLLOWING PATHS CORRECTLY ####
########################################

# ABSOLUTE PATH TO UPC++ INSTALLATION DIRECTORY
export UPCPP_ROOT=/Users/vivek/rice_work/projects/upcxx-repos/upcxx-install

# ABSOLUTE PATH TO HABANERO-C++ BASE DIRECTORY
export HCLIB_BASE=/Users/vivek/rice_work/projects/release/hclib

# ABSOLUTE PATH TO LIBXML2 INSTALLATION
export LIBXML2_INCLUDE=/usr/local/Cellar/libxml2/2.9.2/include/libxml2
export LIBXML2_LIBS=/usr/local/Cellar/libxml2/2.9.2/lib
#export LIBXML2_INCLUDE=/usr/include/libxml2
#export LIBXML2_LIBS=/usr/lib/x86_64-linux-gnu

# ABSOLUTE PATH TO HABANERO-UPC BASE DIRECTORY
export BASE=/Users/vivek/rice_work/projects/release/habanero-upc

#export TBB_MALLOC=/Users/vivek/rice_work/projects/tbb/mac

################################################
#
# DO NOT MODIFY ANYTHING BELOW UNLESS YOU ARE
# CHANGING THE INSTALLATION PATH OF HABANERO-UPC
#
################################################

# COMPILERS (fixed values unless you are building on Edison)
export CXX=mpicxx
export CXX_LINKER=mpicxx
export CC=gcc
export HCUPC_FLAGS="--enable-distWS"

export HCLIB_ROOT=${HCLIB_BASE}/hclib-install
export HABANERO_UPC_ROOT=${BASE}/habanero-upc-install
export LD_LIBRARY_PATH=${HABANERO_UPC_ROOT}/lib:${HCLIB_ROOT}/lib:${LIBXML2_LIBS}:$LD_LIBRARY_PATH

if [ ! -z "${TBB_MALLOC}" ]; then
   export LD_LIBRARY_PATH=${TBB_MALLOC}:$LD_LIBRARY_PATH
fi
