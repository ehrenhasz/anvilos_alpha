if [ $FILE_MODULE ]; then
	return 0
else
	FILE_MODULE=DONE
fi
source cpu.sh
source cpufreq.sh
source governor.sh
test_basic_insmod_rmmod()
{
	printf "** Test: Running ${FUNCNAME[0]} **\n\n"
	printf "Inserting $1 module\n"
	insmod $1
	if [ $? != 0 ]; then
		printf "Insmod $1 failed\n"
		exit;
	fi
	printf "Removing $1 module\n"
	rmmod $1
	if [ $? != 0 ]; then
		printf "rmmod $1 failed\n"
		exit;
	fi
	printf "\n"
}
module_driver_test_single()
{
	printf "** Test: Running ${FUNCNAME[0]} for driver $1 and cpus_hotplug=$2 **\n\n"
	if [ $2 -eq 1 ]; then
		for_each_non_boot_cpu offline_cpu
		printf "\n"
	fi
	printf "Inserting $1 module\n\n"
	insmod $1
	if [ $? != 0 ]; then
		printf "Insmod $1 failed\n"
		return;
	fi
	if [ $2 -eq 1 ]; then
		for_each_non_boot_cpu online_cpu
		printf "\n"
	fi
	cpufreq_basic_tests
	printf "Removing $1 module\n\n"
	rmmod $1
	if [ $? != 0 ]; then
		printf "rmmod $1 failed\n"
		return;
	fi
	for_each_cpu cpu_should_not_have_cpufreq_directory
	printf "\n"
}
module_driver_test()
{
	printf "** Test: Running ${FUNCNAME[0]} **\n\n"
	ls $1 > /dev/null
	if [ $? != 0 ]; then
		printf "$1: not present in `pwd` folder\n"
		return;
	fi
	test_basic_insmod_rmmod $1
	module_driver_test_single $1 0
	module_driver_test_single $1 1
	printf "\n"
}
find_gov_name()
{
	if [ $1 = "cpufreq_ondemand.ko" ]; then
		printf "ondemand"
	elif [ $1 = "cpufreq_conservative.ko" ]; then
		printf "conservative"
	elif [ $1 = "cpufreq_userspace.ko" ]; then
		printf "userspace"
	elif [ $1 = "cpufreq_performance.ko" ]; then
		printf "performance"
	elif [ $1 = "cpufreq_powersave.ko" ]; then
		printf "powersave"
	elif [ $1 = "cpufreq_schedutil.ko" ]; then
		printf "schedutil"
	fi
}
module_governor_test_single()
{
	printf "** Test: Running ${FUNCNAME[0]} for $3 **\n\n"
	backup_governor $3
	printf "Switch from $CUR_GOV to $1\n"
	switch_show_governor $3 $1
	printf "Removing $2 module\n\n"
	rmmod $2
	if [ $? = 0 ]; then
		printf "WARN: rmmod $2 succeeded even if governor is used\n"
		insmod $2
	else
		printf "Pass: unable to remove $2 while it is being used\n\n"
	fi
	printf "Switchback to $CUR_GOV from $1\n"
	restore_governor $3
	printf "\n"
}
module_governor_test()
{
	printf "** Test: Running ${FUNCNAME[0]} **\n\n"
	ls $1 > /dev/null
	if [ $? != 0 ]; then
		printf "$1: not present in `pwd` folder\n"
		return;
	fi
	test_basic_insmod_rmmod $1
	printf "Inserting $1 module\n\n"
	insmod $1
	if [ $? != 0 ]; then
		printf "Insmod $1 failed\n"
		return;
	fi
	for_each_policy module_governor_test_single $(find_gov_name $1) $1
	printf "Removing $1 module\n\n"
	rmmod $1
	if [ $? != 0 ]; then
		printf "rmmod $1 failed\n"
		return;
	fi
	printf "\n"
}
module_test()
{
	printf "** Test: Running ${FUNCNAME[0]} **\n\n"
	ls $1 $2 > /dev/null
	if [ $? != 0 ]; then
		printf "$1 or $2: is not present in `pwd` folder\n"
		return;
	fi
	printf "Inserting $1 module\n\n"
	insmod $1
	if [ $? != 0 ]; then
		printf "Insmod $1 failed\n"
		return;
	fi
	module_governor_test $2
	printf "Removing $1 module\n\n"
	rmmod $1
	if [ $? != 0 ]; then
		printf "rmmod $1 failed\n"
		return;
	fi
	printf "Inserting $2 module\n\n"
	insmod $2
	if [ $? != 0 ]; then
		printf "Insmod $2 failed\n"
		return;
	fi
	module_driver_test $1
	printf "Removing $2 module\n\n"
	rmmod $2
	if [ $? != 0 ]; then
		printf "rmmod $2 failed\n"
		return;
	fi
}
