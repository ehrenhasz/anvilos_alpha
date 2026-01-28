EVALUATE_ONLY=0
ksft_skip=4
if ! uname -m | sed -e s/i.86/x86/ -e s/x86_64/x86/ | grep -q x86; then
	echo "$0 
	exit $ksft_skip
fi
msg="skip all tests:"
if [ $UID != 0 ] && [ $EVALUATE_ONLY == 0 ]; then
    echo $msg please run this as root >&2
    exit $ksft_skip
fi
max_cpus=$(($(nproc)-1))
function run_test () {
	file_ext=$1
	for cpu in `seq 0 $max_cpus`
	do
		echo "launching aperf load on $cpu"
		./aperf $cpu &
	done
	echo "sleeping for 5 seconds"
	sleep 5
	grep MHz /proc/cpuinfo | sort -u > /tmp/result.freqs
	num_freqs=$(wc -l /tmp/result.freqs | awk ' { print $1 } ')
	if [ $num_freqs -ge 2 ]; then
		tail -n 1 /tmp/result.freqs > /tmp/result.$1
	else
		cp /tmp/result.freqs /tmp/result.$1
	fi
	./msr 0 >> /tmp/result.$1
	max_perf_pct=$(cat /sys/devices/system/cpu/intel_pstate/max_perf_pct)
	echo "max_perf_pct $max_perf_pct" >> /tmp/result.$1
	for job in `jobs -p`
	do
		echo "waiting for job id $job"
		wait $job
	done
}
_mkt_freq=$(cat /proc/cpuinfo | grep -m 1 "model name" | awk '{print $NF}')
_mkt_freq=$(echo $_mkt_freq | tr -d [:alpha:][:punct:])
mkt_freq=${_mkt_freq}0
_min_freq=$(cpupower frequency-info -l | tail -1 | awk ' { print $1 } ')
min_freq=$(($_min_freq / 1000))
_max_freq=$(cpupower frequency-info -l | tail -1 | awk ' { print $2 } ')
max_freq=$(($_max_freq / 1000))
[ $EVALUATE_ONLY -eq 0 ] && for freq in `seq $max_freq -100 $min_freq`
do
	echo "Setting maximum frequency to $freq"
	cpupower frequency-set -g powersave --max=${freq}MHz >& /dev/null
	run_test $freq
done
[ $EVALUATE_ONLY -eq 0 ] && cpupower frequency-set -g powersave --max=${max_freq}MHz >& /dev/null
echo "========================================================================"
echo "The marketing frequency of the cpu is $mkt_freq MHz"
echo "The maximum frequency of the cpu is $max_freq MHz"
echo "The minimum frequency of the cpu is $min_freq MHz"
echo "Target Actual Difference MSR(0x199) max_perf_pct" | tr " " "\n" > /tmp/result.tab
for freq in `seq $max_freq -100 $min_freq`
do
	result_freq=$(cat /tmp/result.${freq} | grep "cpu MHz" | awk ' { print $4 } ' | awk -F "." ' { print $1 } ')
	msr=$(cat /tmp/result.${freq} | grep "msr" | awk ' { print $3 } ')
	max_perf_pct=$(cat /tmp/result.${freq} | grep "max_perf_pct" | awk ' { print $2 } ' )
	cat >> /tmp/result.tab << EOF
$freq
$result_freq
$((result_freq - freq))
$msr
$((max_perf_pct * max_freq))
EOF
done
pr -aTt -5 < /tmp/result.tab
exit 0
