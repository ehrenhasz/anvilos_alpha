BCJ=
LZMA2OPTS=
case $SRCARCH in
	x86)            BCJ=--x86 ;;
	powerpc)        BCJ=--powerpc ;;
	ia64)           BCJ=--ia64; LZMA2OPTS=pb=4 ;;
	arm)            BCJ=--arm ;;
	sparc)          BCJ=--sparc ;;
esac
exec $XZ --check=crc32 $BCJ --lzma2=$LZMA2OPTS,dict=32MiB
