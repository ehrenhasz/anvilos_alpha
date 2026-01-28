set -e
usage() {
	echo >&2 "usage: $0 [--abis ABIS] [--prefix PREFIX] INFILE OUTFILE" >&2
	echo >&2
	echo >&2 "  INFILE    input syscall table"
	echo >&2 "  OUTFILE   output header file"
	echo >&2
	echo >&2 "options:"
	echo >&2 "  --abis ABIS        ABI(s) to handle (By default, all lines are handled)"
	echo >&2 "  --prefix PREFIX    The prefix to the macro like __NR_<PREFIX><NAME>"
	exit 1
}
abis=
prefix=
while [ $
do
	case $1 in
	--abis)
		abis=$(echo "($2)" | tr ',' '|')
		shift 2;;
	--prefix)
		prefix=$2
		shift 2;;
	-*)
		echo "$1: unknown option" >&2
		usage;;
	*)
		break;;
	esac
done
if [ $
	usage
fi
infile="$1"
outfile="$2"
guard=_ASM_$(basename "$outfile" |
	sed -e 'y/abcdefghijklmnopqrstuvwxyz/ABCDEFGHIJKLMNOPQRSTUVWXYZ/' \
	-e 's/[^A-Z0-9_]/_/g' -e 's/__/_/g')
grep -E "^[0-9A-Fa-fXx]+[[:space:]]+$abis" "$infile" | sort -n | {
	echo "#ifndef $guard"
	echo "#define $guard"
	echo
	max=0
	while read nr abi name native compat ; do
		max=$nr
	done
	echo "#define __NR_${prefix}syscalls $(($max + 1))"
	echo
	echo "#endif /* $guard */"
} > "$outfile"
