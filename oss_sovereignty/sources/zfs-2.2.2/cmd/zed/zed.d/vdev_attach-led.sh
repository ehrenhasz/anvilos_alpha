[ -f "${ZED_ZEDLET_DIR}/zed.rc" ] && . "${ZED_ZEDLET_DIR}/zed.rc"
. "${ZED_ZEDLET_DIR}/zed-functions.sh"
if [ ! -d /sys/class/enclosure ] && [ ! -d /sys/bus/pci/slots ] ; then
	exit 1
fi
if [ "${ZED_USE_ENCLOSURE_LEDS}" != "1" ] ; then
	exit 2
fi
zed_check_cmd "$ZPOOL" || exit 4
zed_check_cmd awk || exit 5
vdev=""
check_and_set_led()
{
	file="$1"
	val="$2"
	if [ -z "$val" ]; then
		return 0
	fi
	if [ ! -e "$file" ] ; then
		return 3
	fi
	for _ in 1 2 3 4 5; do
		read -r current < "${file}"
		if [ "$current" != "0" ] ; then
			current=1
		fi
		if [ "$current" != "$val" ] ; then
			echo "$val" > "$file"
			zed_log_msg "vdev $vdev set '$file' LED to $val"
		else
			break
		fi
	done
}
path_to_led()
{
	dir=$1
	if [ -f "$dir/fault" ] ; then
		echo "$dir/fault"
	elif [ -f "$dir/attention" ] ; then
		echo "$dir/attention"
	fi
}
state_to_val()
{
	state="$1"
	case "$state" in
		FAULTED|DEGRADED|UNAVAIL|REMOVED)
			echo 1
			;;
		ONLINE)
			echo 0
			;;
		*)
			echo "invalid state: $state"
			;;
	esac
}
nvme_dev_to_slot()
{
	dev="$1"
	read -r address < "/sys/class/block/$dev/device/address"
	find /sys/bus/pci/slots -regex '.*/[0-9]+/address$' | \
		while read -r sys_addr; do
			read -r this_address < "$sys_addr"
			if echo "$address" | grep -Eq ^"$this_address" ; then
				echo "${sys_addr%/*}"
				break
			fi
			done
}
process_pool()
{
	pool="$1"
	ZPOOL_SCRIPTS_AS_ROOT=1 $ZPOOL status -c upath,fault_led "$pool" | grep '/dev/' | (
	rc=0
	while read -r vdev state _ _ _ therest; do
		dev=$(basename "$(echo "$therest" | awk '{print $(NF-1)}')")
		vdev_enc_sysfs_path=$(realpath "/sys/class/block/$dev/device/enclosure_device"*)
		if [ ! -d "$vdev_enc_sysfs_path" ] ; then
			vdev_enc_sysfs_path=$(nvme_dev_to_slot "$dev")
		fi
		current_val=$(echo "$therest" | awk '{print $NF}')
		if [ "$current_val" != "0" ] ; then
			current_val=1
		fi
		if [ -z "$vdev_enc_sysfs_path" ] ; then
			continue
		fi
		led_path=$(path_to_led "$vdev_enc_sysfs_path")
		if [ ! -e "$led_path" ] ; then
			rc=3
			zed_log_msg "vdev $vdev '$led_path' doesn't exist"
			continue
		fi
		val=$(state_to_val "$state")
		if [ "$current_val" = "$val" ] ; then
			continue
		fi
		if ! check_and_set_led "$led_path" "$val"; then
			rc=3
		fi
	done
	exit "$rc"; )
}
if [ -n "$ZEVENT_VDEV_ENC_SYSFS_PATH" ] && [ -n "$ZEVENT_VDEV_STATE_STR" ] ; then
	val=$(state_to_val "$ZEVENT_VDEV_STATE_STR")
	vdev=$(basename "$ZEVENT_VDEV_PATH")
	ledpath=$(path_to_led "$ZEVENT_VDEV_ENC_SYSFS_PATH")
	check_and_set_led "$ledpath" "$val"
else
	poolname=$(zed_guid_to_pool "$ZEVENT_POOL_GUID")
	process_pool "$poolname"
fi
