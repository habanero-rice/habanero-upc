
SUBDIRS = src

AUX_DIST = \
	$(ac_aux_dir)/config.guess		\
	$(ac_aux_dir)/config.sub		\
	$(ac_aux_dir)/install-sh		\
	$(ac_aux_dir)/ltmain.sh			\
	$(ac_aux_dir)/missing			\
	$(ac_aux_dir)/depcomp

library_includedir=$(includedir)

if CPP11
library_include_HEADERS = inc/hclib.h inc/ddf.h inc/phased.h inc/accumulator.h src/inc/runtime-hclib.h src/inc/runtime-support.h src/inc/rt-hclib-def.h inc/hclib-dist.h
else
library_include_HEADERS = inc/hclib.h inc/ddf.h inc/phased.h inc/accumulator.h inc/hclib-dist.h
endif

MAINTAINERCLEANFILES = Makefile.in \
	aclocal.m4 \
	configure  \
	$(AUX_DIST)
