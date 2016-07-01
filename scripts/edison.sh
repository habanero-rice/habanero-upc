#!/bin/bash


########################################
##### SET FOLLOWING PATHS CORRECTLY ####
########################################

# ABSOLUTE PATH TO HABANERO-UPC BASE DIRECTORY
setenv BASE /global/u2/v/vivekk/pgas14/github/habanero-upc

# ABSOLUTE PATH TO UPC++ INSTALLATION DIRECTORY
setenv UPCPP_ROOT /global/u2/v/vivekk/pgas14/hpgmg/upcxx-06262015

# ABSOLUTE PATH TO HABANERO-C++ BASE DIRECTORY
setenv HCLIB_BASE /global/u2/v/vivekk/pgas14/github/hcpp

################################################
#
# DO NOT MODIFY ANYTHING BELOW UNLESS YOU ARE
# CHANGING THE INSTALLATION PATH OF HABANERO-UPC
#
################################################

setenv LIBXML2_INCLUDE /usr/include/libxml2
setenv LIBXML2_LIBS /usr/lib
setenv CXX "CC -DGASNETT_USE_GCC_ATTRIBUTE_MAYALIAS=1"
setenv CXX_LINKER g++
setenv CC cc
setenv HCLIB_ROOT ${HCLIB_BASE}/hclib-install
setenv HABANERO_UPC_ROOT ${BASE}/habanero-upc-install
setenv LD_LIBRARY_PATH ${HABANERO_UPC_ROOT}/lib:${HCLIB_ROOT}/lib:${LIBXML2_LIBS}:$LD_LIBRARY_PATH

#if [ ! -z "${TBB_MALLOC}" ]; then
#   setenv LD_LIBRARY_PATH ${TBB_MALLOC}:$LD_LIBRARY_PATH
#fi
