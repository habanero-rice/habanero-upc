#!/bin/bash

setenv HCPP_BASE  < PATH TO HCPP CHECKED-OUT FROM GITHUB >
setenv UPCPP_ROOT  < PATH TO UPCXX INSTALLATION DIRECTORY >
setenv PATH <PATH TO MPI INSTALLATION DIRECTORY>/bin:$PATH
setenv LD_LIBRARY_PATH <PATH TO MPI INSTALLATION DIRECTORY>/lib:$LD_LIBRARY_PATH
setenv CXX mpicxx
setenv CC gcc

################################################
#
# DO NOT MODIFY ANYTHING BELOW UNLESS YOU ARE
# CHANGING THE INSTALLATION PATH OF HABANERO-UPC
#
################################################

unset HCLIB_ROOT
unset OCR_ROOT
unset OCR_CONFIG
unset HABANERO_UPC_ROOT

cd ..
setenv BASE `pwd`
cd -

setenv HCPP_ROOT ${HCPP_BASE}/hcpp-install
setenv HABANERO_UPC_ROOT ${BASE}/habanero-upc-install
setenv HCLIB_ROOT ${BASE}/others/hclib/hcupc/hclib-install
setenv OCR_ROOT ${BASE}/others/ocr/hcupc/ocr-install
setenv OCR_CONFIG ${BASE}/machine-configs/mach-hcupc-commMaster-1w.cfg
setenv LD_LIBRARY_PATH ${OCR_ROOT}/lib:${HCLIB_ROOT}/lib:${HABANERO_UPC_ROOT}/lib:$LD_LIBRARY_PATH

