skip_if_no_arm_spe_event() {
	perf list | grep -E -q 'arm_spe_[0-9]+//' && return 0
	return 2
}
skip_if_no_arm_spe_event || exit 2
perfdata=$(mktemp /tmp/__perf_test.perf.data.XXXXX)
glb_err=0
cleanup_files()
{
	rm -f ${perfdata}
	rm -f ${perfdata}.old
	exit $glb_err
}
trap cleanup_files EXIT TERM INT
arm_spe_report() {
	if [ $2 = 0 ]; then
		echo "$1: PASS"
	elif [ $2 = 2 ]; then
		echo "$1: SKIPPED"
	else
		echo "$1: FAIL"
		glb_err=$2
	fi
}
perf_script_samples() {
	echo "Looking at perf.data file for dumping samples:"
	events="(ld1-miss|ld1-access|llc-miss|lld-access|tlb-miss|tlb-access|branch-miss|remote-access|memory)"
	perf script -F,-time -i ${perfdata} 2>&1 | \
		grep -E " +$1 +[0-9]+ .* +${events}:(.*:)? +" > /dev/null 2>&1
}
perf_report_samples() {
	echo "Looking at perf.data file for reporting samples:"
	perf report --stdio -i ${perfdata} 2>&1 | \
		grep -E " +[0-9]+\.[0-9]+% +[0-9]+\.[0-9]+% +$1 " > /dev/null 2>&1
}
arm_spe_snapshot_test() {
	echo "Recording trace with snapshot mode $perfdata"
	perf record -o ${perfdata} -e arm_spe// -S \
		-- dd if=/dev/zero of=/dev/null > /dev/null 2>&1 &
	PERFPID=$!
	sleep 1
	kill -USR2 $PERFPID
	kill $PERFPID
	wait $PERFPID
	perf_script_samples dd &&
	perf_report_samples dd
	err=$?
	arm_spe_report "SPE snapshot testing" $err
}
arm_spe_system_wide_test() {
	echo "Recording trace with system-wide mode $perfdata"
	perf record -o - -e dummy -a -B true > /dev/null 2>&1
	if [ $? != 0 ]; then
		arm_spe_report "SPE system-wide testing" 2
		return
	fi
	perf record -o ${perfdata} -e arm_spe// -a --no-bpf-event \
		-- dd if=/dev/zero of=/dev/null count=100000 > /dev/null 2>&1
	perf_script_samples dd &&
	perf_report_samples dd
	err=$?
	arm_spe_report "SPE system-wide testing" $err
}
arm_spe_snapshot_test
arm_spe_system_wide_test
exit $glb_err
