#!/bin/sh


#
# Defining some variables
#

PROJECT_NAME=hclib


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

if [ -z "$OCR_ROOT" ]; then
    echo "error: HCLIB requires Open Community Runtime (OCR)"
    echo " => Set the environment variable OCR_ROOT to point to an OCR install"
    exit 1;
fi

#
# Bootstrap
#
# if install root has been specified, add --prefix option to configure
if [ -n "${INSTALL_ROOT}" ]; then
    INSTALL_ROOT="--prefix=${INSTALL_ROOT}"
else
    INSTALL_ROOT="--prefix=${PWD}/${PROJECT_NAME}-install"
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

HCLIB_CONFIGURE_FLAGS="${HCLIB_CONFIGURE_FLAGS} --enable-cpp11=yes --with-ocr=${OCR_ROOT}"

if [ -n "${PHASERLIB_ROOT}" ]; then
    HCLIB_CONFIGURE_FLAGS+=" --with-phaser=${PHASERLIB_ROOT} "
fi

../configure ${INSTALL_ROOT} ${HCLIB_CONFIGURE_FLAGS}
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


echo "[${PROJECT_NAME}]] Installation complete."
