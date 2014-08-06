#!/bin/bash

export UPCPP_INSTALL=/Users/vivek/rice_work/projects/upcxx-repos/upcxx-install
export CXX=mpicxx

################################################
#
# DO NOT MODIFY ANYTHING BELOW UNLESS YOU ARE
# CHANGING THE INSTALLATION PATH OF HABANERO-UPC
#
################################################

unset HCLIB_ROOT
unset OCR_CONFIG
unset OCR_INSTALL
unset OCR_ROOT
unset HABANERO_UPC_INSTALL

BASE=`pwd`/..

export HABANERO_UPC_INSTALL=${BASE}/habanero-upc-install
export HCLIB_ROOT=${BASE}/others/hclib/hcupc/hclib-install
export OCR_ROOT=${BASE}/others/ocr/hcupc/ocr-install
export OCR_INSTALL=${BASE}/others/ocr/hcupc/ocr-install
export OCR_CONFIG=${BASE}/machine-configs/mach-hcupc-commMaster-1w.cfg
export LD_LIBRARY_PATH=${OCR_ROOT}/lib:${HCLIB_ROOT}/lib:${HABANERO_UPC_INSTALL}/lib:$LD_LIBRARY_PATH
