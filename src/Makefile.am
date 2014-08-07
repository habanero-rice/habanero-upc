include $(UPCPP_INSTALL)/include/upcxx.mak

SUBDIRS =

# Includes folder we need when compiling
HABANEROUPC_INCLUDES_DIR = -I$(top_srcdir)/inc -I$(top_srcdir)/src -I$(HCLIB_ROOT)/include -I$(UPCPP_INSTALL)/include

# cflags: important to define that otherwise we inherit default values too
CXXFLAGS = -g -Wall

# additional cflags 
HABANEROUPC_CXXFLAGS = $(UPCXX_CXXFLAGS)

# Standard am options all the submodules can grab
HABANEROUPC_AM_CXXFLAGS = $(HABANEROUPC_INCLUDES_DIR) $(HABANEROUPC_CXXFLAGS)

# Add warning treated as errors for build
# For modules that do need to bypass this, just use the
# HABANEROUPC_AM_CXXFLAGS variable
AM_CXXFLAGS = $(HABANEROUPC_AM_CXXFLAGS)

lib_LTLIBRARIES = libhcupcpp.la

# These are populated in sub-folder's Makefile.inc
#noinst_LTLIBRARIES =
#libphaser_la_LIBADD =

# build the runtime library
libhcupcpp_la_SOURCES = hcpp-runtime.cpp asyncAwait.cpp asyncPhased.cpp hcupc_spmd.cpp
libhcupcpp_la_CXXFLAGS = $(AM_CXXFLAGS)

MAINTAINERCLEANFILES = Makefile.in
