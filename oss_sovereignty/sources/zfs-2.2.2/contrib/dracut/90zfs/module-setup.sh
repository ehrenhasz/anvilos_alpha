check() {
	[[ "${1}" = "-d" ]] && return 0
	for tool in "zgenhostid" "zpool" "zfs" "mount.zfs"; do
		command -v "${tool}" >/dev/null || return 1
	done
}
depends() {
	echo udev-rules
}
installkernel() {
	instmods -c zfs
}
install() {
	inst_rules 90-zfs.rules 69-vdev.rules 60-zvol.rules
	inst_multiple \
		zgenhostid \
		zfs \
		zpool \
		mount.zfs \
		hostid \
		grep \
		awk \
		tr \
		cut \
		head ||
		{ dfatal "Failed to install essential binaries"; exit 1; }
	if ! ldd "$(command -v zpool)" | grep -qF 'libgcc_s.so' && ldconfig -p 2> /dev/null | grep -qF 'libc.so.6' ; then
		if command -v gcc-config >/dev/null; then
			inst_simple "/usr/lib/gcc/$(s=$(gcc-config -c); echo "${s%-*}/${s##*-}")/libgcc_s.so.1" ||
				{ dfatal "Unable to install libgcc_s.so"; exit 1; }
		elif ! inst_libdir_file "libgcc_s.so*"; then
			inst_simple /usr/lib/gcc/*/*/libgcc_s.so* ||
				{ dfatal "Unable to install libgcc_s.so"; exit 1; }
		fi
	fi
	inst_hook cmdline 95 "${moddir}/parse-zfs.sh"
	if [[ -n "${systemdutildir}" ]]; then
		inst_script "${moddir}/zfs-generator.sh" "${systemdutildir}/system-generators/dracut-zfs-generator"
	fi
	inst_hook pre-mount 90 "${moddir}/zfs-load-key.sh"
	inst_hook mount 98 "${moddir}/mount-zfs.sh"
	inst_hook cleanup 99 "${moddir}/zfs-needshutdown.sh"
	inst_hook shutdown 20 "${moddir}/export-zfs.sh"
	inst_script "${moddir}/zfs-lib.sh" "/lib/dracut-zfs-lib.sh"
	inst_multiple -o -H \
		"/etc/zfs/zpool.cache" \
		"/etc/zfs/vdev_id.conf"
	if ! inst_simple -H /etc/hostid; then
		if HOSTID="$(hostid 2>/dev/null)" && [[ "${HOSTID}" != "00000000" ]]; then
			zgenhostid -o "${initdir}/etc/hostid" "${HOSTID}"
			mark_hostonly /etc/hostid
		fi
	fi
	if dracut_module_included "systemd"; then
		inst_simple "${systemdsystemunitdir}/zfs-import.target"
		systemctl -q --root "${initdir}" add-wants initrd.target zfs-import.target
		inst_simple "${moddir}/zfs-env-bootfs.service" "${systemdsystemunitdir}/zfs-env-bootfs.service"
		systemctl -q --root "${initdir}" add-wants zfs-import.target zfs-env-bootfs.service
		inst_simple "${moddir}/zfs-nonroot-necessities.service" "${systemdsystemunitdir}/zfs-nonroot-necessities.service"
		systemctl -q --root "${initdir}" add-requires initrd-root-fs.target zfs-nonroot-necessities.service
		inst_multiple -o -H \
			"${systemdsystemconfdir}/zfs-import.target" \
			"${systemdsystemconfdir}/zfs-import.target.d/"*.conf
		for _service in \
			"zfs-import-scan.service" \
			"zfs-import-cache.service"; do
			inst_simple "${systemdsystemunitdir}/${_service}"
			systemctl -q --root "${initdir}" add-wants zfs-import.target "${_service}"
			inst_multiple -o -H \
				"${systemdsystemconfdir}/${_service}" \
				"${systemdsystemconfdir}/${_service}.d/"*.conf
		done
		for _service in \
			"zfs-snapshot-bootfs.service" \
			"zfs-rollback-bootfs.service"; do
			inst_simple "${moddir}/${_service}" "${systemdsystemunitdir}/${_service}"
			systemctl -q --root "${initdir}" add-wants initrd.target "${_service}"
			inst_multiple -o -H \
				"${systemdsystemconfdir}/${_service}" \
				"${systemdsystemconfdir}/${_service}.d/"*.conf
		done
		inst_simple "${moddir}/import-opts-generator.sh" "${systemdutildir}/system-environment-generators/zfs-import-opts.sh"
	fi
}
