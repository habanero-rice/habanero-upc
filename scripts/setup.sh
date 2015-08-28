#!/bin/bash

########################################
##### SET FOLLOWING PATHS CORRECTLY ####
########################################

# ABSOLUTE PATH TO UPC++ INSTALLATION DIRECTORY
export UPCPP_ROOT=/Users/vivek/rice_work/projects/upcxx-repos/upcxx-install

# ABSOLUTE PATH TO HABANERO-C++ BASE DIRECTORY
export HCPP_BASE=/Users/vivek/rice_work/projects/release/hcpp

# MPI C++ COMPILER
export CXX=mpicxx

# C COMPILER
export CC=gcc

# ABSOLUTE PATH TO LIBXML2 INSTALLATION
export LIBXML2=/usr/local/Cellar/libxml2/2.9.2

# ABSOLUTE PATH TO HABANERO-UPC BASE DIRECTORY
export BASE=/Users/vivek/rice_work/projects/release/habanero-upc

#export TBB_MALLOC=/Users/vivek/rice_work/projects/tbb/mac

################################################
#
# DO NOT MODIFY ANYTHING BELOW UNLESS YOU ARE
# CHANGING THE INSTALLATION PATH OF HABANERO-UPC
#
################################################

export HCPP_ROOT=${HCPP_BASE}/hcpp-install
export HABANERO_UPC_ROOT=${BASE}/habanero-upc-install
export LD_LIBRARY_PATH=${HABANERO_UPC_ROOT}/lib:${HCPP_ROOT}/lib:$LD_LIBRARY_PATH

if [ ! -z "${TBB_MALLOC}" ]; then
   export LD_LIBRARY_PATH=${TBB_MALLOC}:$LD_LIBRARY_PATH
fi