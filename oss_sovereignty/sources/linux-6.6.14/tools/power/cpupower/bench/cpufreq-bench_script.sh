UP_THRESHOLD="60 80 95"
SAMPLING_RATE="20000 80000"
function measure()
{
    local -i up_threshold_set
    local -i sampling_rate_set
    for up_threshold in $UP_THRESHOLD;do
	for sampling_rate in $SAMPLING_RATE;do
	    echo $up_threshold >/sys/devices/system/cpu/cpu0/cpufreq/ondemand/up_threshold
	    echo $sampling_rate >/sys/devices/system/cpu/cpu0/cpufreq/ondemand/sampling_rate
	    up_threshold_set=$(cat /sys/devices/system/cpu/cpu0/cpufreq/ondemand/up_threshold)
	    sampling_rate_set=$(cat /sys/devices/system/cpu/cpu0/cpufreq/ondemand/sampling_rate)
	    if [ ${up_threshold_set} -eq ${up_threshold} ];then
		echo "up_threshold: $up_threshold, set in sysfs: ${up_threshold_set}"
	    else
		echo "WARNING: Tried to set up_threshold: $up_threshold, set in sysfs: ${up_threshold_set}"
	    fi
	    if [ ${sampling_rate_set} -eq ${sampling_rate} ];then
		echo "sampling_rate: $sampling_rate, set in sysfs: ${sampling_rate_set}"
	    else
		echo "WARNING: Tried to set sampling_rate: $sampling_rate, set in sysfs: ${sampling_rate_set}"
	    fi
	    cpufreq-bench -o /var/log/cpufreq-bench/up_threshold_${up_threshold}_sampling_rate_${sampling_rate}
	done
    done
}
function create_plots()
{
    local command
    for up_threshold in $UP_THRESHOLD;do
	command="cpufreq-bench_plot.sh -o \"sampling_rate_${SAMPLING_RATE}_up_threshold_${up_threshold}\" -t \"Ondemand sampling_rate: ${SAMPLING_RATE} comparison - Up_threshold: $up_threshold %\""
	for sampling_rate in $SAMPLING_RATE;do
	    command="${command} /var/log/cpufreq-bench/up_threshold_${up_threshold}_sampling_rate_${sampling_rate}/* \"sampling_rate = $sampling_rate\""
	done
	echo $command
	eval "$command"
	echo
    done
    for sampling_rate in $SAMPLING_RATE;do
	command="cpufreq-bench_plot.sh -o \"up_threshold_${UP_THRESHOLD}_sampling_rate_${sampling_rate}\" -t \"Ondemand up_threshold: ${UP_THRESHOLD} % comparison - sampling_rate: $sampling_rate\""
	for up_threshold in $UP_THRESHOLD;do
	    command="${command} /var/log/cpufreq-bench/up_threshold_${up_threshold}_sampling_rate_${sampling_rate}/* \"up_threshold = $up_threshold\""
	done
	echo $command
	eval "$command"
	echo
    done
    command="cpufreq-bench_plot.sh -o \"up_threshold_${UP_THRESHOLD}_sampling_rate_${SAMPLING_RATE}\" -t \"Ondemand up_threshold: ${UP_THRESHOLD} and sampling_rate ${SAMPLING_RATE} comparison\""
    for sampling_rate in $SAMPLING_RATE;do
	for up_threshold in $UP_THRESHOLD;do
	    command="${command} /var/log/cpufreq-bench/up_threshold_${up_threshold}_sampling_rate_${sampling_rate}/* \"up_threshold = $up_threshold - sampling_rate = $sampling_rate\""
	done
    done
    echo "$command"
    eval "$command"
}
measure
create_plots
