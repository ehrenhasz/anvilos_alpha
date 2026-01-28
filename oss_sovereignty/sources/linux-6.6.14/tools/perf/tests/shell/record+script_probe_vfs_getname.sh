. "$(dirname "$0")/lib/probe.sh"
skip_if_no_perf_probe || exit 2
. "$(dirname "$0")/lib/probe_vfs_getname.sh"
record_open_file() {
	echo "Recording open file:"
	skip_no_probe_record_support "probe:vfs_getname*"
	[ $? -eq 2 ] && return 2
	perf record -o ${perfdata} -e probe:vfs_getname\* touch $file
}
perf_script_filenames() {
	echo "Looking at perf.data file for vfs_getname records for the file we touched:"
	perf script -i ${perfdata} | \
	grep -E " +touch +[0-9]+ +\[[0-9]+\] +[0-9]+\.[0-9]+: +probe:vfs_getname[_0-9]*: +\([[:xdigit:]]+\) +pathname=\"${file}\""
}
add_probe_vfs_getname || skip_if_no_debuginfo
err=$?
if [ $err -ne 0 ] ; then
	exit $err
fi
perfdata=$(mktemp /tmp/__perf_test.perf.data.XXXXX)
file=$(mktemp /tmp/temporary_file.XXXXX)
record_open_file && perf_script_filenames
err=$?
rm -f ${perfdata}
rm -f ${file}
cleanup_probe_vfs_getname
exit $err
