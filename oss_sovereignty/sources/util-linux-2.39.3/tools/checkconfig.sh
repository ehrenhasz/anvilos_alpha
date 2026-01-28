die() {
	echo "error: $1"
	exit 1
}
usage() {
	echo "Usage:"
	echo " $0 <top_srcdir> <srcfile> [<srcfile> ...]"
	echo "Example:"
	echo " find . -name '*.c' | xargs $0 \$(git rev-parse --show-toplevel)"
}
if [ $
	usage
	exit 1
fi
srcdir=$1
config="$srcdir/config.h.in"
[ -d "$srcdir" ] || die "$srcdir: not such directory."
[ -f "$config" ] || die "$config: not such file."
shift
while [ "$
	srcfile=$1
	shift
	[ ! -f "$srcfile" ] && continue;
	DEFINES=$(sed -n -e 's/.*[ \t(]\+\(HAVE_[[:alnum:]]\+[^ \t);]*\).*/\1/p' \
			 -e 's/.*[ \t(]\+\(ENABLE_[[:alnum:]]\+[^ \t);]*\).*/\1/p' \
                         $srcfile | sort -u)
	[ -z "$DEFINES" ] && continue
	for d in $DEFINES; do
		case $d in
		HAVE_CONFIG_H) continue
		   ;;
		*) grep -q "$d\( \|\>\)" $config || \
		     echo $(echo $srcfile | sed 's:\\'$srcdir/'::') ": $d"
	           ;;
		esac
	done
done
