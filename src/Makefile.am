include $(UPCPP_ROOT)/include/upcxx.mak

SUBDIRS =

# Includes folder we need when compiling

HABANEROUPC_INCLUDES_DIR = -I$(top_srcdir)/inc -I$(top_srcdir)/src -I$(HCLIB_ROOT)/include -I$(HCPP_ROOT)/include -I$(UPCPP_ROOT)/include

# cflags: important to define that otherwise we inherit default values too
CXXFLAGS = -g -Wall

# additional cflags 
HABANEROUPC_CXXFLAGS = $(UPCXX_CXXFLAGS)

# Standard am options all the submodules can grab
HABANEROUPC_AM_CXXFLAGS = $(HABANEROUPC_INCLUDES_DIR) $(HABANEROUPC_CXXFLAGS)

# Add warning treated as errors for build
# For modules that do need to bypass this, just use the
AM_CXXFLAGS = $(HABANEROUPC_AM_CXXFLAGS)

lib_LTLIBRARIES = libhcupcpp.la

# build the runtime library
libhcupcpp_la_SOURCES = hcupc_spmd.cpp

libhcupcpp_la_CXXFLAGS = $(AM_CXXFLAGS)

MAINTAINERCLEANFILES = Makefile.in
