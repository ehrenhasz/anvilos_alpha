. /lib/dracut-zfs-lib.sh
decode_root_args || return 0
GENERATOR_FILE=/run/systemd/generator/sysroot.mount
GENERATOR_EXTENSION=/run/systemd/generator/sysroot.mount.d/zfs-enhancement.conf
if [ -e "$GENERATOR_FILE" ] && [ -e "$GENERATOR_EXTENSION" ]; then
	info "ZFS: Delegating root mount to sysroot.mount at al."
	rm -f "$hookdir"/mount/*zfs*
	return
fi
info "ZFS: No sysroot.mount exists or zfs-generator did not extend it."
info "ZFS: Mounting root with the traditional mount-zfs.sh instead."
ask_for_password() {
    tries="$1"
    prompt="$2"
    cmd="$3"
    {
        flock -s 9
        if plymouth --ping 2>/dev/null; then
            plymouth ask-for-password \
                --prompt "$prompt" --number-of-tries="$tries" | \
                eval "$cmd"
            ret=$?
        else
            i=1
            while [ "$i" -le "$tries" ]; do
                printf "%s [%i/%i]:" "$prompt" "$i" "$tries" >&2
                eval "$cmd" && ret=0 && break
                ret=$?
                i=$((i+1))
                printf '\n' >&2
            done
            unset i
        fi
    } 9>/.console_lock
    [ "$ret" -ne 0 ] && echo "Wrong password" >&2
    return "$ret"
}
modprobe zfs 2>/dev/null
udevadm settle
ZFS_DATASET=
ZFS_POOL=
if [ "${root}" = "zfs:AUTO" ] ; then
	if ! ZFS_DATASET="$(zpool get -Ho value bootfs | grep -m1 -vFx -)"; then
		zpool import -N -a ${ZPOOL_IMPORT_OPTS}
		if ! ZFS_DATASET="$(zpool get -Ho value bootfs | grep -m1 -vFx -)"; then
			warn "ZFS: No bootfs attribute found in importable pools."
			zpool export -aF
			rootok=0
			return 1
		fi
	fi
	info "ZFS: Using ${ZFS_DATASET} as root."
fi
ZFS_DATASET="${ZFS_DATASET:-${root}}"
ZFS_POOL="${ZFS_DATASET%%/*}"
if ! zpool get -Ho value name "${ZFS_POOL}" > /dev/null 2>&1; then
    info "ZFS: Importing pool ${ZFS_POOL}..."
    if ! zpool import -N ${ZPOOL_IMPORT_OPTS} "${ZFS_POOL}"; then
        warn "ZFS: Unable to import pool ${ZFS_POOL}"
        rootok=0
        return 1
    fi
fi
if [ "$(zpool get -Ho value feature@encryption "${ZFS_POOL}")" = 'active' ]; then
	ENCRYPTIONROOT="$(zfs get -Ho value encryptionroot "${ZFS_DATASET}")"
	if ! [ "${ENCRYPTIONROOT}" = "-" ]; then
		KEYSTATUS="$(zfs get -Ho value keystatus "${ENCRYPTIONROOT}")"
		if [ "$KEYSTATUS" = "unavailable" ]; then
			ask_for_password \
				5 \
				"Encrypted ZFS password for ${ENCRYPTIONROOT}: " \
				"zfs load-key '${ENCRYPTIONROOT}'"
		fi
	fi
fi
info "ZFS: Mounting dataset ${ZFS_DATASET}..."
if ! mount_dataset "${ZFS_DATASET}"; then
  rootok=0
  return 1
fi
