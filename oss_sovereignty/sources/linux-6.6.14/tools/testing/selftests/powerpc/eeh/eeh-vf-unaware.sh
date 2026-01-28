. ./eeh-functions.sh
eeh_test_prep 
vf_list="$(eeh_enable_vfs)";
if $? != 0 ; then
	log "No usable VFs found. Skipping EEH unaware VF test"
	exit $KSELFTESTS_SKIP;
fi
log "Enabled VFs: $vf_list"
failed=0
for vf in $vf_list ; do
	log "Testing $vf"
	if eeh_can_recover $vf ; then
		log "Driver for $vf supports error recovery. Unbinding..."
		echo "$vf" > /sys/bus/pci/devices/$vf/driver/unbind
	fi
	log "Breaking $vf..."
	if ! eeh_one_dev $vf ; then
		log "$vf failed to recover"
		failed="$((failed + 1))"
	fi
done
eeh_disable_vfs
test "$failed" != 0
exit $?;
