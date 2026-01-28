[ -f "${ZED_ZEDLET_DIR}/zed.rc" ] && . "${ZED_ZEDLET_DIR}/zed.rc"
. "${ZED_ZEDLET_DIR}/zed-functions.sh"
if [ ! -d /sys/class/enclosure ] ; then
	exit 1
fi
if [ "${ZED_POWER_OFF_ENCLOUSRE_SLOT_ON_FAULT}" != "1" ] ; then
	exit 2
fi
if [ "$ZEVENT_VDEV_STATE_STR" != "FAULTED" ] ; then
	exit 3
fi
if [ ! -f "$ZEVENT_VDEV_ENC_SYSFS_PATH/power_status" ] ; then
	exit 4
fi
for i in $(seq 1 20) ; do
	echo "off" | tee "$ZEVENT_VDEV_ENC_SYSFS_PATH/power_status"
	for j in $(seq 1 5) ; do
		if [ "$(cat $ZEVENT_VDEV_ENC_SYSFS_PATH/power_status)" == "off" ] ; then
			break 2
		fi
		sleep 0.1
	done
done
if [ "$(cat $ZEVENT_VDEV_ENC_SYSFS_PATH/power_status)" != "off" ] ; then
	exit 5
fi
zed_log_msg "powered down slot $ZEVENT_VDEV_ENC_SYSFS_PATH for $ZEVENT_VDEV_PATH"
