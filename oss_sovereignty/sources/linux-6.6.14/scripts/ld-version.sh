set -e
get_canonical_version()
{
	IFS=.
	set -- $1
	echo $((10000 * $1 + 100 * ${2:-0} + ${3:-0}))
}
orig_args="$@"
IFS='
'
set -- $(LC_ALL=C "$@" --version)
IFS=' '
set -- $1
min_tool_version=$(dirname $0)/min-tool-version.sh
if [ "$1" = GNU -a "$2" = ld ]; then
	shift $(($# - 1))
	version=$1
	min_version=$($min_tool_version binutils)
	name=BFD
	disp_name="GNU ld"
elif [ "$1" = GNU -a "$2" = gold ]; then
	echo "gold linker is not supported as it is not capable of linking the kernel proper." >&2
	exit 1
else
	while [ $# -gt 1 -a "$1" != "LLD" ]; do
		shift
	done
	if [ "$1" = LLD ]; then
		version=$2
		min_version=$($min_tool_version llvm)
		name=LLD
		disp_name=LLD
	else
		echo "$orig_args: unknown linker" >&2
		exit 1
	fi
fi
version=${version%-*}
cversion=$(get_canonical_version $version)
min_cversion=$(get_canonical_version $min_version)
if [ "$cversion" -lt "$min_cversion" ]; then
	echo >&2 "***"
	echo >&2 "*** Linker is too old."
	echo >&2 "***   Your $disp_name version:    $version"
	echo >&2 "***   Minimum $disp_name version: $min_version"
	echo >&2 "***"
	exit 1
fi
echo $name $cversion
