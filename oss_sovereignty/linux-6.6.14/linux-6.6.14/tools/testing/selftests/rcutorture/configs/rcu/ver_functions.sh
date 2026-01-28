rcutorture_param_n_barrier_cbs () {
	if echo $1 | grep -q "rcutorture\.n_barrier_cbs"
	then
		:
	else
		echo rcutorture.n_barrier_cbs=4
	fi
}
rcutorture_param_onoff () {
	if ! bootparam_hotplug_cpu "$1" && configfrag_hotplug_cpu "$2"
	then
		echo CPU-hotplug kernel, adding rcutorture onoff. 1>&2
		echo rcutorture.onoff_interval=1000 rcutorture.onoff_holdoff=30
	fi
}
rcutorture_param_stat_interval () {
	if echo $1 | grep -q "rcutorture\.stat_interval"
	then
		:
	else
		echo rcutorture.stat_interval=15
	fi
}
per_version_boot_params () {
	echo	`rcutorture_param_onoff "$1" "$2"` \
		`rcutorture_param_n_barrier_cbs "$1"` \
		`rcutorture_param_stat_interval "$1"` \
		rcutorture.shutdown_secs=$3 \
		rcutorture.test_no_idle_hz=1 \
		rcutorture.verbose=1 \
		$1
}
