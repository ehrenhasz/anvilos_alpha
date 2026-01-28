CC=gcc
STATIC=-static
trap 'rm -f conftest.c' INT HUP TERM
rm -f findssl.log
cat >conftest.c <<EOD
int main(){printf("0x%08xL\n", SSLeay());}
EOD
DEFAULT_LIBPATH=/usr/lib:/usr/local/lib
LIBPATH=${LIBPATH:=$DEFAULT_LIBPATH}
LD_LIBRARY_PATH=${LD_LIBRARY_PATH:=$DEFAULT_LIBPATH}
LIBRARY_PATH=${LIBRARY_PATH:=$DEFAULT_LIBPATH}
export LIBPATH LD_LIBRARY_PATH LIBRARY_PATH
if which ls >/dev/null 2>/dev/null; then
    : which is defined
else
    which () {
	saveIFS="$IFS"
	IFS=:
	for p in $PATH; do
	    if test -x "$p/$1" -a -f "$p/$1"; then
		IFS="$saveIFS"
		echo "$p/$1"
		return 0
	    fi
	done
	IFS="$saveIFS"
	return 1
    }
fi
echo Searching for OpenSSL header files.
if [ -x "`which locate`" ]
then
	headers=`locate opensslv.h`
else
	headers=`find / -name opensslv.h -print 2>/dev/null`
fi
for header in $headers
do
	ver=`awk '/OPENSSL_VERSION_NUMBER/{printf \$3}' $header`
	echo "$ver $header"
done
echo
echo Searching for OpenSSL shared library files.
if [ -x "`which locate`" ]
then
	libraries=`locate libcrypto.s`
else
	libraries=`find / -name 'libcrypto.s*' -print 2>/dev/null`
fi
for lib in $libraries
do
	(echo "Trying libcrypto $lib" >>findssl.log
	dir=`dirname $lib`
	LIBPATH="$dir:$LIBPATH"
	LD_LIBRARY_PATH="$dir:$LIBPATH"
	LIBRARY_PATH="$dir:$LIBPATH"
	export LIBPATH LD_LIBRARY_PATH LIBRARY_PATH
	${CC} -o conftest conftest.c $lib 2>>findssl.log
	if [ -x ./conftest ]
	then
		ver=`./conftest 2>/dev/null`
		rm -f ./conftest
		echo "$ver $lib"
	fi)
done
echo
echo Searching for OpenSSL static library files.
if [ -x "`which locate`" ]
then
	libraries=`locate libcrypto.a`
else
	libraries=`find / -name libcrypto.a -print 2>/dev/null`
fi
for lib in $libraries
do
	libdir=`dirname $lib`
	echo "Trying libcrypto $lib" >>findssl.log
	${CC} ${STATIC} -o conftest conftest.c -L${libdir} -lcrypto 2>>findssl.log
	if [ -x ./conftest ]
	then
		ver=`./conftest 2>/dev/null`
		rm -f ./conftest
		echo "$ver $lib"
	fi
done
rm -f conftest.c
