[ -f "${ZED_ZEDLET_DIR}/zed.rc" ] && . "${ZED_ZEDLET_DIR}/zed.rc"
. "${ZED_ZEDLET_DIR}/zed-functions.sh"
zed_exit_if_ignoring_this_event
msg="eid=${ZEVENT_EID} class=${ZEVENT_SUBCLASS}"
if [ "${ZED_SYSLOG_DISPLAY_GUIDS}" = "1" ]; then
    [ -n "${ZEVENT_POOL_GUID}" ] && msg="${msg} pool_guid=${ZEVENT_POOL_GUID}"
    [ -n "${ZEVENT_VDEV_GUID}" ] && msg="${msg} vdev_guid=${ZEVENT_VDEV_GUID}"
else
    [ -n "${ZEVENT_POOL}" ] && msg="${msg} pool='${ZEVENT_POOL}'"
    [ -n "${ZEVENT_VDEV_PATH}" ] && msg="${msg} vdev=${ZEVENT_VDEV_PATH
fi
[ -n "${ZEVENT_POOL_STATE_STR}" ] && [ "$ZEVENT_POOL_STATE" -ne 0 ] && \
    msg="${msg} pool_state=${ZEVENT_POOL_STATE_STR}"
[ -n "${ZEVENT_VDEV_STATE_STR}" ]  && msg="${msg} vdev_state=${ZEVENT_VDEV_STATE_STR}"
[ -n "${ZEVENT_CKSUM_ALGORITHM}" ] && msg="${msg} algorithm=${ZEVENT_CKSUM_ALGORITHM}"
[ -n "${ZEVENT_ZIO_SIZE}" ]        && msg="${msg} size=${ZEVENT_ZIO_SIZE}"
[ -n "${ZEVENT_ZIO_OFFSET}" ]      && msg="${msg} offset=${ZEVENT_ZIO_OFFSET}"
[ -n "${ZEVENT_ZIO_PRIORITY}" ]    && msg="${msg} priority=${ZEVENT_ZIO_PRIORITY}"
[ -n "${ZEVENT_ZIO_ERR}" ]         && msg="${msg} err=${ZEVENT_ZIO_ERR}"
[ -n "${ZEVENT_ZIO_FLAGS}" ]       && msg="${msg} flags=$(printf '0x%x' "${ZEVENT_ZIO_FLAGS}")"
[ -n "${ZEVENT_ZIO_DELAY}" ] && [ "$ZEVENT_ZIO_DELAY" -gt 10000000 ] && \
    msg="${msg} delay=$((ZEVENT_ZIO_DELAY / 1000000))ms"
[ -n "${ZEVENT_ZIO_OBJSET}" ] && \
    msg="${msg} bookmark=${ZEVENT_ZIO_OBJSET}:${ZEVENT_ZIO_OBJECT}:${ZEVENT_ZIO_LEVEL}:${ZEVENT_ZIO_BLKID}"
zed_log_msg "${msg}"
exit 0
