set -e
DIR=$(dirname "$0")
ret=0
expect_success()
{
	title="$1"
	shift
	echo "" >&2
	echo "$title ..." >&2
	set +e
	"$@"
	rc=$?
	set -e
	case "$rc" in
	0)
		echo "ok: $title succeeded" >&2
		;;
	1)
		echo "FAIL: $title should work" >&2
		ret=$(( ret + 1 ))
		;;
	*)
		echo "FAIL: something else went wrong" >&2
		ret=$(( ret + 1 ))
		;;
	esac
}
expect_failure()
{
	title="$1"
	shift
	echo "" >&2
	echo "$title ..." >&2
	set +e
	"$@"
	rc=$?
	set -e
	case "$rc" in
	0)
		echo "FAIL: $title unexpectedly worked" >&2
		ret=$(( ret + 1 ))
		;;
	1)
		echo "ok: $title correctly failed" >&2
		;;
	*)
		echo "FAIL: something else went wrong" >&2
		ret=$(( ret + 1 ))
		;;
	esac
}
do_splice()
{
	filename="$1"
	bytes="$2"
	expected="$3"
	report="$4"
	out=$("$DIR"/splice_read "$filename" "$bytes" | cat)
	if [ "$out" = "$expected" ] ; then
		echo "      matched $report" >&2
		return 0
	else
		echo "      no match: '$out' vs $report" >&2
		return 1
	fi
}
test_splice()
{
	filename="$1"
	echo "  checking $filename ..." >&2
	full=$(cat "$filename")
	rc=$?
	if [ $rc -ne 0 ] ; then
		return 2
	fi
	two=$(echo "$full" | grep -m1 . | cut -c-2)
	echo "    splicing 4096 bytes ..." >&2
	if ! do_splice "$filename" 4096 "$full" "full read" ; then
		return 1
	fi
	echo "    splicing 2 bytes ..." >&2
	if ! do_splice "$filename" 2 "$two" "'$two'" ; then
		return 1
	fi
	return 0
}
expect_failure "proc_single_open(), seq_read() splice" test_splice /proc/$$/limits
expect_failure "special open(), seq_read() splice" test_splice /proc/$$/comm
expect_success "proc_handler: proc_dointvec_minmax() splice" test_splice /proc/sys/fs/nr_open
expect_success "proc_handler: proc_dostring() splice" test_splice /proc/sys/kernel/modprobe
expect_success "proc_handler: special read splice" test_splice /proc/sys/kernel/version
if ! [ -d /sys/module/test_module/sections ] ; then
	expect_success "test_module kernel module load" modprobe test_module
fi
expect_success "kernfs attr splice" test_splice /sys/module/test_module/coresize
expect_success "kernfs binattr splice" test_splice /sys/module/test_module/sections/.init.text
exit $ret
