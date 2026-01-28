set -e
get_c_compiler_info()
{
	cat <<- EOF | "$@" -E -P -x c - 2>/dev/null
	Clang	__clang_major__  __clang_minor__  __clang_patchlevel__
	GCC	__GNUC__  __GNUC_MINOR__  __GNUC_PATCHLEVEL__
	unknown
	EOF
}
get_canonical_version()
{
	IFS=.
	set -- $1
	echo $((10000 * $1 + 100 * $2 + $3))
}
orig_args="$@"
set -- $(get_c_compiler_info "$@")
name=$1
min_tool_version=$(dirname $0)/min-tool-version.sh
case "$name" in
GCC)
	version=$2.$3.$4
	min_version=$($min_tool_version gcc)
	;;
Clang)
	version=$2.$3.$4
	min_version=$($min_tool_version llvm)
	;;
*)
	echo "$orig_args: unknown C compiler" >&2
	exit 1
	;;
esac
cversion=$(get_canonical_version $version)
min_cversion=$(get_canonical_version $min_version)
if [ "$cversion" -lt "$min_cversion" ]; then
	echo >&2 "***"
	echo >&2 "*** C compiler is too old."
	echo >&2 "***   Your $name version:    $version"
	echo >&2 "***   Minimum $name version: $min_version"
	echo >&2 "***"
	exit 1
fi
echo $name $cversion
