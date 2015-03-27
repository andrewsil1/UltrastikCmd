AC_INIT([ultrastikcmd], [0.2], [andrewsi@outlook.com])

AC_COPYRIGHT([Please see the ./COPYING file for license information.])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])

AC_CANONICAL_HOST    # needs to be called at some point earlier
AM_CONDITIONAL([ADDARGP_LIBS],
    [case $host_os in darwin*) true;; *) false;; esac])

AC_ARG_WITH(libhid,
    [ --with-libhid=<path> prefix of libhid installation. e.g. /usr/local or /usr],
    [LIBHID_PREFIX=$with_libhid],
    [LIBHID_PREFIX=/usr]
)

AC_SUBST(LIBHID_PREFIX)
LIBHID_LIBS="-L${LIBHID_PREFIX}/lib -lhid"
LIBHID_CFLAGS="-I${LIBHID_PREFIX}/include"
AC_SUBST(LIBHID_LIBS)
AC_SUBST(LIBHID_CFLAGS)

AC_ARG_WITH(libusb,
    [ --with-libusb=<path> prefix of libusb installation. e.g. /usr/local or /usr],
    [LIBUSB_PREFIX=$with_libusb],
    [LIBUSB_PREFIX=/usr]
)

AC_SUBST(LIBUSB_PREFIX)
LIBUSB_LIBS="-L${LIBUSB_PREFIX}/lib -lusb"
LIBUSB_CFLAGS="-I${LIBUSB_PREFIX}/include"
AC_SUBST(LIBUSB_LIBS)
AC_SUBST(LIBUSB_CFLAGS)

AC_ARG_WITH(libargp,
    [ --with-libargp=<path> prefix of libargp installation. e.g. /usr/local or /usr],
    [LIBARGP_PREFIX=$with_libargp],
    [LIBARGP_PREFIX=/usr]
)

AC_SUBST(LIBARGP_PREFIX)
LIBARGP_LIBS="-L${LIBARGP_PREFIX}/lib -largp"
LIBARGP_CFLAGS="-I${LIBARGP_PREFIX}/include"
AC_SUBST(LIBARGP_LIBS)
AC_SUBST(LIBARGP_CFLAGS)

AC_PROG_CC
AC_PROG_CXX
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_FILES([
	Makefile
    src/Makefile
    maps/Makefile
    tools/Makefile
])

AC_OUTPUT


