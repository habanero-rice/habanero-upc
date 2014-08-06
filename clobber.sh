#!/bin/sh

if [ `env | grep OCR_ROOT | wc -l` -ne 1 ]
then
  echo "Please source setMeFirst.sh first"
  exit
fi

#
# Cleanup the project 
#

BASE=`pwd`

cd $BASE/others/ocr/hcupc
./clobber.sh

cd $BASE/others/hclib/hcupc
./clobber.sh

cd $BASE

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

