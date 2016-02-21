#!/bin/sh

set -e

BASE=`pwd`

echo 
echo "USAGE: HCUPC_FLAGS=<configure flags> ./install.sh"
echo
echo "========================================================================="
echo "                HabaneroUPC++ configure flags"
echo "========================================================================="
echo "1) Inter-node work-stealing runtime"
echo "  --enable-distWS (enable distributed work-stealing support)"
echo "2) Disable Statistics and assertion checks"
echo "  --enable-production"
echo "========================================================================="

#Flag to build without dependencies (i.e. just build HabaneroUPC++ and not HCLIB)
withoutDependencies=1
# by default build everything (HCLIB and HabaneroUPC++)
all=1
if [ $# -ge 1 ]; then
  if [ $1 -eq ${withoutDependencies} ]; then
    all=0
  fi
fi

if [ $all -eq 1 ]; then
    echo "================== BUILDING HCLIB ==================="
    cd ${HCLIB_BASE}
    ./clobber.sh
    echo "HCLIB Build Flags: ${HCUPC_FLAGS} --enable-hupcpp"
    HCLIB_FLAGS="${HCUPC_FLAGS} --enable-hupcpp" ./install.sh
fi

echo "================== BUILDING HABANERO_UPC =============="
cd $BASE

#
# Defining some variables
#

PROJECT_NAME=habanero-upc

check_error()
{
    if [ $# -gt 2 ]; then
	echo "Error in check_error call";
	exit 1;
    fi;
    ERRCODE="$1";
    if [ "$ERRCODE" = "0" ]; then
	return 0;
    fi;
    if [ $# -eq 2 ]; then
	ERRMESSAGE="$2";
    else
	ERRMESSAGE="Error";
    fi;
    echo "[${PROJECT_NAME}] $ERRMESSAGE";
    exit $ERRCODE;
}


#
# Bootstrap, Configure, Make and Install
#

if [ -z "$NPROC" ]; then 
    NPROC=1
fi

INSTALL_DIR=

#
# Bootstrap
#
# if install root has been specified, add --prefix option to configure
if [ -n "${INSTALL_ROOT}" ]; then
    INSTALL_ROOT="--prefix=${INSTALL_ROOT}"
    INSTALL_DIR="${INSTALL_ROOT}"
else
    INSTALL_ROOT="--prefix=${PWD}/habanero-upc-install"
    INSTALL_DIR="${PWD}/habanero-upc-install"
fi

echo "[${PROJECT_NAME}] Bootstrap..."

./bootstrap.sh
check_error "$?" "Bootstrap failed";


#
# Configure
#
echo "[${PROJECT_NAME}]] Configure..."

COMPTREE=$PWD/compileTree
if [ ! -d "${COMPTREE}" ]; then
    mkdir ${COMPTREE}
fi

cd ${COMPTREE}

../configure ${INSTALL_ROOT} ${HCUPC_FLAGS}
check_error "$?" "Configure failed";


#
# Make
#
echo "[${PROJECT_NAME}]] Make..."
make -j${NPROC}
check_error "$?" "Build failed";


#
# Make install
#
# if install root has been specified, perform make install
echo "[${PROJECT_NAME}]] Make install... to ${INSTALL_ROOT}"
make -j${NPROC} install
check_error "$?" "Installation failed";

if [ `echo ${HCUPC_FLAGS} | grep enable\-crtFull | wc -l` -ne 0 ]; then
	cp -f ${BASE}/inc/distws/hcupc_spmd.mak ${INSTALL_DIR}/include/
fi

if [ `echo ${HCUPC_FLAGS} | grep enable\-crtHalf | wc -l` -ne 0 ]; then
	cp -f ${BASE}/inc/crtNoDistws/hcupc_spmd.mak ${INSTALL_DIR}/include/
fi

if [ `echo ${HCUPC_FLAGS} | grep enable\-hclib | wc -l` -ne 0 ]; then
	cp -f ${BASE}/inc/ocrHclib/hcupc_spmd.mak ${INSTALL_DIR}/include/
fi

echo "[${PROJECT_NAME}]] Installation complete."
