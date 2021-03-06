#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT(gipfel, 0.4.0, Johannes.Hofmann@gmx.de)
AM_INIT_AUTOMAKE
AC_CONFIG_SRCDIR([src/Panorama.H])
AC_CONFIG_HEADER(config.h)

CPPFLAGS="-Wall -g -I/usr/local/include -DSTD_DATADIR=\\\"$datadir\\\" -DDOCDIR=\\\"$docdir\\\" $CPPFLAGS"
LDFLAGS="-g $LDFLAGS"

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

AC_LANG_CPLUSPLUS

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_OFF_T
AC_TYPE_PID_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_TYPE_SIGNAL
AC_CHECK_FUNCS([strchr strdup strrchr strstr mkstemp fsync strsep])

# Check for fltk
AC_PATH_PROG(FLTKCONFIG,fltk-config)
if test "x$FLTKCONFIG" = x; then
	echo "fltk-config not found"
	exit 1
fi

CXXFLAGS="`$FLTKCONFIG --use-images --cflags` $CXXFLAGS"
LIBS="`$FLTKCONFIG --use-images --ldflags` $LIBS"

# Check for gsl
AC_PATH_PROG(GSLCONFIG,gsl-config)
if test "x$GSLCONFIG" = x; then
	echo "gsl-config not found"
	exit 1
fi

CXXFLAGS="`$GSLCONFIG --cflags` $CXXFLAGS"
LIBS="`$GSLCONFIG --libs` $LIBS"

# Check for libtiff
AC_CHECK_HEADERS([tiffio.h], [], [echo "Error: tiffio.h not found."; exit 1;])
AC_CHECK_LIB([tiff], [TIFFOpen], [], [echo "Error: libtiff.so not found."; exit 1;])

# Check for exiv2
AC_CHECK_HEADERS([exiv2/exif.hpp], [], [echo "Error: exiv2/exif.hpp not found."; exit 1;])
LIBS="-lexiv2 $LIBS"

AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT
