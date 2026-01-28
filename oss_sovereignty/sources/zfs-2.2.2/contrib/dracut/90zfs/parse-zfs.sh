. /lib/dracut-zfs-lib.sh
spl_hostid=$(getarg spl_hostid=)
if [ -n "${spl_hostid}" ] ; then
	info "ZFS: Using hostid from command line: ${spl_hostid}"
	zgenhostid -f "${spl_hostid}"
elif [ -f "/etc/hostid" ] ; then
	info "ZFS: Using hostid from /etc/hostid: $(hostid)"
else
	warn "ZFS: No hostid found on kernel command line or /etc/hostid."
	warn "ZFS: Pools may not import correctly."
fi
if decode_root_args; then
	if [ "$root" = "zfs:AUTO" ]; then
		info "ZFS: Boot dataset autodetected from bootfs=."
	else
		info "ZFS: Boot dataset is ${root}."
	fi
	rootok=1
	if [ -n "${wait_for_zfs}" ]; then
		ln -s null /dev/root
		echo '[ -e /dev/zfs ]' > "${hookdir}/initqueue/finished/zfs.sh"
	fi
else
	info "ZFS: no ZFS-on-root."
fi
