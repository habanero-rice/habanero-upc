include $(UPCPP_ROOT)/include/upcxx.mak

SUBDIRS =

HABANEROUPC_FLAGS = -I$(top_srcdir)/inc -I$(UPCPP_ROOT)/include -I$(HCPP_ROOT)/include $(UPCXX_CXXFLAGS) -DHUPCPP -DHCPP_COMM_WORKER

# Includes folder we need when compiling

if DIST_WS
DIST_WS_FLAGS = -DDIST_WS
else
DIST_WS_FLAGS =
endif

if PRODUCTION_SETTINGS
PRODUCTION_SETTINGS_FLAGS =   
else
PRODUCTION_SETTINGS_FLAGS = -DHC_ASSERTION_CHECK -DHC_COMM_WORKER_STATS
endif

# cflags: important to define that otherwise we inherit default values too
CXXFLAGS = -g -Wall -std=c++11

# Standard am options all the submodules can grab
HABANEROUPC_CXXFLAGS = $(HABANEROUPC_FLAGS) $(PRODUCTION_SETTINGS_FLAGS) $(DIST_WS_FLAGS)

# Add warning treated as errors for build
# For modules that do need to bypass this, just use the
AM_CXXFLAGS = $(HABANEROUPC_CXXFLAGS)

lib_LTLIBRARIES = libhcupcpp.la

# build the runtime library
libhcupcpp_la_SOURCES = hcupc_spmd.cpp hcupc_spmd_distWS_common.cpp hcupc_spmd-semiConcDeque.cpp hcupc_spmd-distHPT.cpp hcupc_spmd_stats.cpp hcupc_spmd_dddf.cpp
libhcupcpp_la_CXXFLAGS = $(AM_CXXFLAGS)

MAINTAINERCLEANFILES = Makefile.in
