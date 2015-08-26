#!/bin/sh

if [ `env | grep HCPP_ROOT | wc -l` -ne 1 ]
then
  echo "Please source setMeFirst.sh first"
  exit
fi

#
# Cleanup the project 
#

BASE=`pwd`

echo "USAGE: ./clobber.sh <[1 -->do not clean dependencies]>"

#Flag to clean without dependencies (i.e. just clean HabaneroUPC++ and not OCR and HCLib)
withoutDependencies=1
# by default clean everything (OCR, HCLib and HabaneroUPC++)
all=1
if [ $# -eq 1 ]; then
  if [ $1 -eq ${withoutDependencies} ]; then
    all=0
  fi
fi

if [ $all -eq 1 ]; then
  cd $hcpp
  ./clobber.sh
  cd $BASE
fi

rm -Rf compileTree
rm -Rf config/* autom4te.cache aclocal.m4 configure COPYING depcomp config.log config.status libtool
rm -f Makefile Makefile.in 

for file in `find src -name Makefile.in`; do
    rm -Rf $file
done

for file in `find src -name Makefile`; do
    rm -Rf $file
done

for file in `find src -regex '.*\.o$'`; do
    rm -Rf $file
done

for file in `find src -regex '.*\.lo$'`; do
    rm -Rf $file
done

for file in `find src -regex '.*\.la$'`; do
    rm -Rf $file
done

for file in `find src -regex '.*deps$'`; do
    rm -Rf $file
done

for file in `find src -regex '.*libs$'`; do
    rm -Rf $file
done

rm -rf install habanero-upc-install
