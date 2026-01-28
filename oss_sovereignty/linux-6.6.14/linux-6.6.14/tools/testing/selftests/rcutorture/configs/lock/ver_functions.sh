locktorture_param_onoff () {
	if ! bootparam_hotplug_cpu "$1" && configfrag_hotplug_cpu "$2"
	then
		echo CPU-hotplug kernel, adding locktorture onoff. 1>&2
		echo locktorture.onoff_interval=3 locktorture.onoff_holdoff=30
	fi
}
per_version_boot_params () {
	echo	`locktorture_param_onoff "$1" "$2"` \
		locktorture.stat_interval=15 \
		locktorture.shutdown_secs=$3 \
		locktorture.verbose=1 \
		$1
}
