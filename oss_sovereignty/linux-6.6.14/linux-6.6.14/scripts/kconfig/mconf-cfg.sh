cflags=$1
libs=$2
PKG="ncursesw"
PKG2="ncurses"
if [ -n "$(command -v ${HOSTPKG_CONFIG})" ]; then
	if ${HOSTPKG_CONFIG} --exists $PKG; then
		${HOSTPKG_CONFIG} --cflags ${PKG} > ${cflags}
		${HOSTPKG_CONFIG} --libs ${PKG} > ${libs}
		exit 0
	fi
	if ${HOSTPKG_CONFIG} --exists ${PKG2}; then
		${HOSTPKG_CONFIG} --cflags ${PKG2} > ${cflags}
		${HOSTPKG_CONFIG} --libs ${PKG2} > ${libs}
		exit 0
	fi
fi
if [ -f /usr/include/ncursesw/ncurses.h ]; then
	echo -D_GNU_SOURCE -I/usr/include/ncursesw > ${cflags}
	echo -lncursesw > ${libs}
	exit 0
fi
if [ -f /usr/include/ncurses/ncurses.h ]; then
	echo -D_GNU_SOURCE -I/usr/include/ncurses > ${cflags}
	echo -lncurses > ${libs}
	exit 0
fi
if echo '
	echo -D_GNU_SOURCE > ${cflags}
	echo -lncurses > ${libs}
	exit 0
fi
echo >&2 "*"
echo >&2 "* Unable to find the ncurses package."
echo >&2 "* Install ncurses (ncurses-devel or libncurses-dev"
echo >&2 "* depending on your distribution)."
echo >&2 "*"
echo >&2 "* You may also need to install ${HOSTPKG_CONFIG} to find the"
echo >&2 "* ncurses installed in a non-default location."
echo >&2 "*"
exit 1
