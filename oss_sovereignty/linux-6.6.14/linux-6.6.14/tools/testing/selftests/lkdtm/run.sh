set -e
TRIGGER=/sys/kernel/debug/provoke-crash/DIRECT
CLEAR_ONCE=/sys/kernel/debug/clear_warn_once
KSELFTEST_SKIP_TEST=4
if [ ! -r $TRIGGER ] ; then
	/sbin/modprobe -q lkdtm || true
	if [ ! -r $TRIGGER ] ; then
		echo "Cannot find $TRIGGER (missing CONFIG_LKDTM?)"
	else
		echo "Cannot write $TRIGGER (need to run as root?)"
	fi
	exit $KSELFTEST_SKIP_TEST
fi
test=$(basename $0 .sh)
line=$(grep -E '^
if [ -z "$line" ]; then
	echo "Skipped: missing test '$test' in tests.txt"
	exit $KSELFTEST_SKIP_TEST
fi
if ! grep -E -q '^'"$test"'$' "$TRIGGER" ; then
	echo "Skipped: test '$test' missing in $TRIGGER!"
	exit $KSELFTEST_SKIP_TEST
fi
test=$(echo "$line" | cut -d" " -f1)
if echo "$line" | grep -q ' ' ; then
	expect=$(echo "$line" | cut -d" " -f2-)
else
	expect=""
fi
if echo "$test" | grep -q '^
	test=$(echo "$test" | cut -c2-)
	if [ -z "$expect" ]; then
		expect="crashes entire system"
	fi
	echo "Skipping $test: $expect"
	exit $KSELFTEST_SKIP_TEST
fi
repeat=1
if [ -z "$expect" ]; then
	expect="call trace:"
else
	if echo "$expect" | grep -q '^repeat:' ; then
		repeat=$(echo "$expect" | cut -d' ' -f1 | cut -d: -f2)
		expect=$(echo "$expect" | cut -d' ' -f2-)
	fi
fi
LOG=$(mktemp --tmpdir -t lkdtm-log-XXXXXX)
DMESG=$(mktemp --tmpdir -t lkdtm-dmesg-XXXXXX)
cleanup() {
	rm -f "$LOG" "$DMESG"
}
trap cleanup EXIT
if [ -w $CLEAR_ONCE ] ; then
	echo 1 > $CLEAR_ONCE
fi
dmesg > "$DMESG"
for i in $(seq 1 $repeat); do
	echo "$test" | cat >"$TRIGGER" || true
done
dmesg | comm --nocheck-order -13 "$DMESG" - > "$LOG" || true
cat "$LOG"
if grep -E -qi "$expect" "$LOG" ; then
	echo "$test: saw '$expect': ok"
	exit 0
else
	if grep -E -qi XFAIL: "$LOG" ; then
		echo "$test: saw 'XFAIL': [SKIP]"
		exit $KSELFTEST_SKIP_TEST
	else
		echo "$test: missing '$expect': [FAIL]"
		exit 1
	fi
fi
