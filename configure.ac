#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.


#Autoconf requirements
AC_PREREQ([2.67])
AC_INIT([Cheetah Distributed Computing Framework], [0.2], [p110390@fct.unl.pt], [cheetah], [http://asc.di.fct.unl.pt/cheetah])

#Information on the package
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_SRCDIR([src/libcheetah-common/cheetah-common.c])
AC_CONFIG_HEADERS([config/config.h])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([foreign silent-rules subdir-objects 1.11 -Wall -Werror])

# Checks for programs.
AC_LANG([C])
ACX_MPI([], [AC_MSG_ERROR([No MPI installation detected.])])
CC="$MPICC"
LIBS="$MPILIBS $LIBS"

#  C Compiler characteristics
AM_PROG_CC_C_O
AC_PROG_CC_C99
AS_IF([test "x$ac_cv_prog_cc_c99" == "xno"],
  [AC_MSG_ERROR([Cannot set the C compiler to C99 mode.])]
)

# Checks for libraries.
#OpenCL
AC_SEARCH_LIBS([clEnqueueNDRangeKernel], [OpenCL], [],
  [AC_MSG_ERROR([Unable to find an OpenCL implementation library.])]
)

#m
AC_SEARCH_LIBS([sqrtf]. [m], [],
  [AC_MSG_ERROR([Unable to find libm.])]
)

# Checks for header files.
AC_FUNC_ALLOCA
AC_CHECK_HEADERS([limits.h stdlib.h string.h sys/time.h])

#OpenCL
AC_CHECK_HEADER([CL/cl.h], [opencl_headers_found=yes], [] )
AS_IF([test "x$opencl_headers_found" != "xyes"],
  [AC_MSG_ERROR([Unable to find OpenCL headers])]
)


# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([gettimeofday memmove sqrt])

#AC_PROG_LIBTOOL #Deprecated!
LT_INIT([disable-static])

#Define makefiles
#Using the non-recursive version!
AC_CONFIG_FILES([Makefile])

AC_OUTPUT
