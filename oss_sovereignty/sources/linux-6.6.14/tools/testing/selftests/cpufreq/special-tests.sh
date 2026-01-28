if [ $FILE_SPECIAL ]; then
	return 0
else
	FILE_SPECIAL=DONE
fi
source cpu.sh
source cpufreq.sh
source governor.sh
__simple_lockdep()
{
	__switch_governor $1 "ondemand"
	local ondir=$(find_gov_directory $1 "ondemand")
	if [ -z $ondir ]; then
		printf "${FUNCNAME[0]}Ondemand directory not created, quit"
		return
	fi
	cat $ondir/*
	__switch_governor $1 "conservative"
}
simple_lockdep()
{
	printf "** Test: Running ${FUNCNAME[0]} **\n"
	for_each_policy __simple_lockdep
}
__concurrent_lockdep()
{
	for i in `seq 0 100`; do
		__simple_lockdep $1
	done
}
concurrent_lockdep()
{
	printf "** Test: Running ${FUNCNAME[0]} **\n"
	for_each_policy_concurrent __concurrent_lockdep
}
quick_shuffle()
{
	for I in `seq 1000`
	do
		echo ondemand | sudo tee $CPUFREQROOT/policy*/scaling_governor &
		echo userspace | sudo tee $CPUFREQROOT/policy*/scaling_governor &
	done
}
governor_race()
{
	printf "** Test: Running ${FUNCNAME[0]} **\n"
	for I in `seq 8`
	do
		quick_shuffle &
	done
}
hotplug_with_updates_cpu()
{
	local filepath="$CPUROOT/$1/cpufreq"
	__switch_governor_for_cpu $1 "ondemand"
	for i in `seq 1 5000`
	do
		reboot_cpu $1
	done &
	local freqs=$(cat $filepath/scaling_available_frequencies)
	local oldfreq=$(cat $filepath/scaling_min_freq)
	for j in `seq 1 5000`
	do
		for freq in $freqs; do
			echo $freq > $filepath/scaling_min_freq
		done
	done
	echo $oldfreq > $filepath/scaling_min_freq
}
hotplug_with_updates()
{
	for_each_non_boot_cpu hotplug_with_updates_cpu
}
