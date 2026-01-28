scftorture_param_onoff () {
	if ! bootparam_hotplug_cpu "$1" && configfrag_hotplug_cpu "$2"
	then
		echo CPU-hotplug kernel, adding scftorture onoff. 1>&2
		echo scftorture.onoff_interval=1000 scftorture.onoff_holdoff=30
	fi
}
per_version_boot_params () {
	echo	`scftorture_param_onoff "$1" "$2"` \
		scftorture.stat_interval=15 \
		scftorture.shutdown_secs=$3 \
		scftorture.verbose=1 \
		$1
}
