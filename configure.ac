# check required autoconf version
AC_PREREQ([2.59])

# define distribution properties
AC_INIT([habanero-upc], [0.1], [hc-dev-l@mailman.rice.edu])

# check if srcdir is correct
AC_CONFIG_SRCDIR([inc/hcupc_spmd.h])

AC_CONFIG_AUX_DIR([config])

AM_INIT_AUTOMAKE([1.9 tar-ustar no-define foreign dist-bzip2])

AC_COPYRIGHT([todo_copyright_info])

# configure script revision
AC_REVISION([$Revision: 0.1 $])


### BUG FIX ###
# For shared object's memory address, UPC++ assumes these objects 
# are aligned on different ranks.
# However, in dynamically loaded objects the loader may put the UPC++ 
# objects in different memory location on different process.
# This is not desired and will lead to segfault.  
# Hence, to resolve statically linked libraries are only to be created
# i.e., -fPIC should never be used. The following fix ensures this.
##############
#AC_PROG_CXX
#AC_PROG_LIBTOOL
#AC_LANG(C++)
LT_INIT([disable-shared])
dnl LT_INIT()
LT_LANG([C++])

### ENABLE DISTRIBUTED WORK_STEALING SUPPORT
AC_ARG_ENABLE(distWS,
    AS_HELP_STRING([--enable-distWS],
    [enable dist. W.S. support (Default is false)]),
    [with_distWS=$enableval],
    [with_distWS=no;])

AS_IF([test "x$with_distWS" != xno],
      [ AC_MSG_NOTICE([Enabled Dist. W.S. support]) ],
      [ AC_MSG_NOTICE([Disabled Dist. W.S. support]) ])

AM_CONDITIONAL(DIST_WS, test "x$with_distWS" != xno)
### End Dist WS

### ENABLE PRODUCTION SETTINGS
AC_ARG_ENABLE(production,
    AS_HELP_STRING([--enable-production],
    [disable assertion and statistics (Default is true)]),
    [with_production=$enableval],
    [with_production=no;])

AS_IF([test "x$with_production" != xno],
      [ AC_MSG_NOTICE([Production settings enabled]) ],
      [ AC_MSG_NOTICE([Production settings disabled]) ])

AM_CONDITIONAL(PRODUCTION_SETTINGS, test "x$with_production" != xno)
### End PRODUCTION settings

# Variable substitutions.
AC_SUBST([ac_aux_dir])
AC_SUBST([abs_top_srcdir])

#
# Config file to process
#
AC_CONFIG_FILES([
Makefile
src/Makefile
])

# Generates and runs config.status, which in turn creates the makefiles 
# and any other files resulting from configuration
AC_OUTPUT

