command -v getarg >/dev/null || . /lib/dracut-lib.sh || . /usr/lib/dracut/modules.d/99base/dracut-lib.sh
TAB="	"
ZPOOL_IMPORT_OPTS=
if getargbool 0 zfs_force -y zfs.force -y zfsforce; then
    warn "ZFS: Will force-import pools if necessary."
    ZPOOL_IMPORT_OPTS=-f
fi
_mount_dataset_cb() {
    mount -o zfsutil -t zfs "${1}" "${NEWROOT}${2}"
}
mount_dataset() {
    dataset="${1}"
    mountpoint="$(zfs get -H -o value mountpoint "${dataset}")"
    ret=0
    if [ "${mountpoint}" = "legacy" ] ; then
        mount -t zfs "${dataset}" "${NEWROOT}" || ret=$?
    else
        mount -o zfsutil -t zfs "${dataset}" "${NEWROOT}" || ret=$?
        if [ "$ret" = "0" ]; then
            for_relevant_root_children "${dataset}" _mount_dataset_cb || ret=$?
        fi
    fi
    return "${ret}"
}
for_relevant_root_children() {
    dataset="${1}"
    exec="${2}"
    zfs list -t filesystem -Ho name,mountpoint,canmount -r "${dataset}" |
        (
            _ret=0
            while IFS="${TAB}" read -r dataset mountpoint canmount; do
                [ "$canmount" != "on" ] && continue
                case "$mountpoint" in
                    /etc|/bin|/lib|/lib??|/libx32|/usr)
                        "${exec}" "${dataset}" "${mountpoint}" || _ret=$?
                        ;;
                    *)
                        ;;
                esac
            done
            exit "${_ret}"
        )
}
decode_root_args() {
    if [ -n "$rootfstype" ]; then
        [ "$rootfstype" = zfs ]
        return
    fi
    xroot=$(getarg root=)
    rootfstype=$(getarg rootfstype=)
    case "$xroot" in
        ""|zfs|zfs:|zfs:AUTO)
            root=zfs:AUTO
            rootfstype=zfs
            return 0
            ;;
        ZFS=*|zfs:*)
            root="${xroot
            root="${root
            root=$(echo "$root" | tr '+' ' ')
            rootfstype=zfs
            return 0
            ;;
    esac
    if [ "$rootfstype" = "zfs" ]; then
        case "$xroot" in
            "") root=zfs:AUTO ;;
            *)  root=$(echo "$xroot" | tr '+' ' ') ;;
        esac
        return 0
    fi
    return 1
}
