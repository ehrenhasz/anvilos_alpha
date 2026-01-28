. "$(dirname $0)"/lib/probe.sh
skip_if_no_perf_probe || exit 2
skip_if_no_perf_trace || exit 2
. "$(dirname $0)"/lib/probe_vfs_getname.sh
trace_open_vfs_getname() {
	evts="$(echo "$(perf list syscalls:sys_enter_open* 2>/dev/null | grep -E 'open(at)? ' | sed -r 's/.*sys_enter_([a-z]+) +\[.*$/\1/')" | sed ':a;N;s:\n:,:g')"
	perf trace -e $evts touch $file 2>&1 | \
	grep -E " +[0-9]+\.[0-9]+ +\( +[0-9]+\.[0-9]+ ms\): +touch/[0-9]+ open(at)?\((dfd: +CWD, +)?filename: +\"?${file}\"?, +flags: CREAT\|NOCTTY\|NONBLOCK\|WRONLY, +mode: +IRUGO\|IWUGO\) += +[0-9]+$"
}
add_probe_vfs_getname || skip_if_no_debuginfo
err=$?
if [ $err -ne 0 ] ; then
	exit $err
fi
file=$(mktemp /tmp/temporary_file.XXXXX)
export PERF_CONFIG=/dev/null
trace_open_vfs_getname
err=$?
rm -f ${file}
cleanup_probe_vfs_getname
exit $err
