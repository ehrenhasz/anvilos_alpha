glb_err=0
skip_if_no_cs_etm_event() {
	perf list | grep -q 'cs_etm//' && return 0
	return 2
}
skip_if_no_cs_etm_event || exit 2
perfdata=$(mktemp /tmp/__perf_test.perf.data.XXXXX)
file=$(mktemp /tmp/temporary_file.XXXXX)
cleanup_files()
{
	rm -f ${perfdata}
	rm -f ${file}
	rm -f "${perfdata}.old"
	trap - EXIT TERM INT
	exit $glb_err
}
trap cleanup_files EXIT TERM INT
record_touch_file() {
	echo "Recording trace (only user mode) with path: CPU$2 => $1"
	rm -f $file
	perf record -o ${perfdata} -e cs_etm/@$1/u --per-thread \
		-- taskset -c $2 touch $file > /dev/null 2>&1
}
perf_script_branch_samples() {
	echo "Looking at perf.data file for dumping branch samples:"
	perf script -F,-time -i ${perfdata} 2>&1 | \
		grep -E " +$1 +[0-9]+ .* +branches:(.*:)? +" > /dev/null 2>&1
}
perf_report_branch_samples() {
	echo "Looking at perf.data file for reporting branch samples:"
	perf report --stdio -i ${perfdata} 2>&1 | \
		grep -E " +[0-9]+\.[0-9]+% +[0-9]+\.[0-9]+% +$1 " > /dev/null 2>&1
}
perf_report_instruction_samples() {
	echo "Looking at perf.data file for instruction samples:"
	perf report --itrace=i20i --stdio -i ${perfdata} 2>&1 | \
		grep -E " +[0-9]+\.[0-9]+% +$1" > /dev/null 2>&1
}
arm_cs_report() {
	if [ $2 != 0 ]; then
		echo "$1: FAIL"
		glb_err=$2
	else
		echo "$1: PASS"
	fi
}
is_device_sink() {
	echo "$1" | grep -E -q -v "tpiu"
	if [ $? -eq 0 ] && [ -e "$1/enable_sink" ]; then
		pmu_dev="/sys/bus/event_source/devices/cs_etm/sinks/$2"
		if ! [ -f $pmu_dev ]; then
			echo "PMU doesn't support $pmu_dev"
		fi
		return 0
	fi
	return 1
}
arm_cs_iterate_devices() {
	for dev in $1/connections/out\:*; do
		! [ -d $dev ] && continue;
		path=`readlink -f $dev`
		device_name=$(basename $path)
		if is_device_sink $path $device_name; then
			record_touch_file $device_name $2 &&
			perf_script_branch_samples touch &&
			perf_report_branch_samples touch &&
			perf_report_instruction_samples touch
			err=$?
			arm_cs_report "CoreSight path testing (CPU$2 -> $device_name)" $err
		fi
		arm_cs_iterate_devices $dev $2
	done
}
arm_cs_etm_traverse_path_test() {
	for dev in /sys/bus/coresight/devices/etm*; do
		cpu=`cat $dev/cpu`
		arm_cs_iterate_devices $dev $cpu
	done
}
arm_cs_etm_system_wide_test() {
	echo "Recording trace with system wide mode"
	perf record -o ${perfdata} -e cs_etm// -a -- ls > /dev/null 2>&1
	perf_script_branch_samples perf &&
	perf_report_branch_samples perf &&
	perf_report_instruction_samples perf
	err=$?
	arm_cs_report "CoreSight system wide testing" $err
}
arm_cs_etm_snapshot_test() {
	echo "Recording trace with snapshot mode"
	perf record -o ${perfdata} -e cs_etm// -S \
		-- dd if=/dev/zero of=/dev/null > /dev/null 2>&1 &
	PERFPID=$!
	sleep 1
	kill -USR2 $PERFPID
	kill $PERFPID
	wait $PERFPID
	perf_script_branch_samples dd &&
	perf_report_branch_samples dd &&
	perf_report_instruction_samples dd
	err=$?
	arm_cs_report "CoreSight snapshot testing" $err
}
arm_cs_etm_basic_test() {
	echo "Recording trace with '$*'"
	perf record -o ${perfdata} "$@" -- ls > /dev/null 2>&1
	perf_script_branch_samples ls &&
	perf_report_branch_samples ls &&
	perf_report_instruction_samples ls
	err=$?
	arm_cs_report "CoreSight basic testing with '$*'" $err
}
arm_cs_etm_traverse_path_test
arm_cs_etm_system_wide_test
arm_cs_etm_snapshot_test
arm_cs_etm_basic_test -e cs_etm/timestamp=0/ --per-thread
arm_cs_etm_basic_test -e cs_etm/timestamp=1/ --per-thread
arm_cs_etm_basic_test -e cs_etm/timestamp=0/ -a
arm_cs_etm_basic_test -e cs_etm/timestamp=1/ -a
arm_cs_etm_basic_test -e cs_etm/timestamp=0/
arm_cs_etm_basic_test -e cs_etm/timestamp=1/
exit $glb_err
