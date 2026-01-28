set -ef
FSLIST="/etc/zfs/zfs-list.cache/${ZEVENT_POOL}"
FSLIST_TMP="/var/run/zfs-list.cache@${ZEVENT_POOL}"
[ -w "${FSLIST}" ] || exit 0
[ -f "${ZED_ZEDLET_DIR}/zed.rc" ] && . "${ZED_ZEDLET_DIR}/zed.rc"
. "${ZED_ZEDLET_DIR}/zed-functions.sh"
[ "$ZEVENT_SUBCLASS" != "history_event" ] && exit 0
zed_check_cmd "${ZFS}" sort diff
[ "${ZEVENT_HISTORY_DSNAME%@*}" = "${ZEVENT_HISTORY_DSNAME}" ] || exit 0
abort_alter() {
  zed_log_msg "Error updating zfs-list.cache for ${ZEVENT_POOL}!"
  zed_unlock "${FSLIST}"
}
finished() {
  zed_unlock "${FSLIST}"
  trap - EXIT
  exit 0
}
case "${ZEVENT_HISTORY_INTERNAL_NAME}" in
    create|"finish receiving"|import|destroy|rename)
      ;;
    export)
        zed_lock "${FSLIST}"
        trap abort_alter EXIT
        echo > "${FSLIST}"
        finished
      ;;
    set|inherit)
        case "${ZEVENT_HISTORY_INTERNAL_STR%%=*}" in
            canmount|mountpoint|atime|relatime|devices|exec|readonly| \
              setuid|nbmand|encroot|keylocation|org.openzfs.systemd:requires| \
              org.openzfs.systemd:requires-mounts-for| \
              org.openzfs.systemd:before|org.openzfs.systemd:after| \
              org.openzfs.systemd:wanted-by|org.openzfs.systemd:required-by| \
              org.openzfs.systemd:nofail|org.openzfs.systemd:ignore \
            ) ;;
            *) exit 0 ;;
        esac
      ;;
    *)
        exit 0
      ;;
esac
zed_lock "${FSLIST}"
trap abort_alter EXIT
PROPS="name,mountpoint,canmount,atime,relatime,devices,exec\
,readonly,setuid,nbmand,encroot,keylocation\
,org.openzfs.systemd:requires,org.openzfs.systemd:requires-mounts-for\
,org.openzfs.systemd:before,org.openzfs.systemd:after\
,org.openzfs.systemd:wanted-by,org.openzfs.systemd:required-by\
,org.openzfs.systemd:nofail,org.openzfs.systemd:ignore"
"${ZFS}" list -H -t filesystem -o "${PROPS}" -r "${ZEVENT_POOL}" > "${FSLIST_TMP}"
sort "${FSLIST_TMP}" -o "${FSLIST_TMP}"
diff -q "${FSLIST_TMP}" "${FSLIST}" || cat "${FSLIST_TMP}" > "${FSLIST}"
rm -f "${FSLIST_TMP}"
finished
