set -e
get_canonical_version()
{
	IFS=.
	set -- $1
	echo $((10000 * $1 + 100 * ${2:-0} + ${3:-0}))
}
check_integrated_as()
{
	while [ $# -gt 0 ]; do
		if [ "$1" = -fintegrated-as ]; then
			echo LLVM 0
			exit 0
		fi
		shift
	done
}
check_integrated_as "$@"
orig_args="$@"
IFS='
'
set -- $(LC_ALL=C "$@" -Wa,--version -c -x assembler-with-cpp /dev/null -o /dev/null 2>/dev/null)
IFS=' '
set -- $1
min_tool_version=$(dirname $0)/min-tool-version.sh
if [ "$1" = GNU -a "$2" = assembler ]; then
	shift $(($# - 1))
	version=$1
	min_version=$($min_tool_version binutils)
	name=GNU
else
	echo "$orig_args: unknown assembler invoked" >&2
	exit 1
fi
version=${version%-*}
cversion=$(get_canonical_version $version)
min_cversion=$(get_canonical_version $min_version)
if [ "$cversion" -lt "$min_cversion" ]; then
	echo >&2 "***"
	echo >&2 "*** Assembler is too old."
	echo >&2 "***   Your $name assembler version:    $version"
	echo >&2 "***   Minimum $name assembler version: $min_version"
	echo >&2 "***"
	exit 1
fi
echo $name $cversion
