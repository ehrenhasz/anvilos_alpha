[ -f "${ZED_ZEDLET_DIR}/zed.rc" ] && . "${ZED_ZEDLET_DIR}/zed.rc"
. "${ZED_ZEDLET_DIR}/zed-functions.sh"
[ -n "${ZEVENT_POOL}" ] || exit 9
[ -n "${ZEVENT_SUBCLASS}" ] || exit 9
zed_check_cmd "${ZPOOL}" || exit 9
umask 077
note_subject="ZFS ${ZEVENT_SUBCLASS} event for ${ZEVENT_POOL} on $(hostname)"
note_pathname="$(mktemp)"
{
    echo "ZFS has finished a trim:"
    echo
    echo "   eid: ${ZEVENT_EID}"
    echo " class: ${ZEVENT_SUBCLASS}"
    echo "  host: $(hostname)"
    echo "  time: ${ZEVENT_TIME_STRING}"
    "${ZPOOL}" status -t "${ZEVENT_POOL}"
} > "${note_pathname}"
zed_notify "${note_subject}" "${note_pathname}"; rv=$?
rm -f "${note_pathname}"
exit "${rv}"
