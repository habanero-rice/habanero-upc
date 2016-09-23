
SUBDIRS = src

AUX_DIST = \
	$(ac_aux_dir)/config.guess		\
	$(ac_aux_dir)/config.sub		\
	$(ac_aux_dir)/install-sh		\
	$(ac_aux_dir)/ltmain.sh			\
	$(ac_aux_dir)/missing			\
	$(ac_aux_dir)/depcomp

library_includedir=$(includedir)

if DIST_WS
library_include_HEADERS = inc/hcpp-distws/hcupc_spmd.mak inc/hcupc_spmd.h inc/hcupc_spmd-main.h inc/hcupc_spmd-util.h inc/hcupc_spmd_common_methods.h inc/hcupc_spmd-commTask.h inc/hcupc_spmd-runtime-interface.h inc/hcupc_spmd-asyncAwait.h inc/hcupc_spmd-dddf.h
else
library_include_HEADERS = inc/hcpp/hcupc_spmd.mak inc/hcupc_spmd.h inc/hcupc_spmd-main.h inc/hcupc_spmd-util.h inc/hcupc_spmd_common_methods.h inc/hcupc_spmd-commTask.h inc/hcupc_spmd-runtime-interface.h inc/hcupc_spmd-asyncAwait.h inc/hcupc_spmd-dddf.h
endif

MAINTAINERCLEANFILES = Makefile.in \
	aclocal.m4 \
	configure  \
	$(AUX_DIST)
