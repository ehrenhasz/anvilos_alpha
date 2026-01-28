VERBOSE="${VERBOSE:-1}"
IKCONFIG="/tmp/config-`uname -r`"
KERNEL_IMAGE="/boot/vmlinuz-`uname -r`"
SECURITYFS=$(grep "securityfs" /proc/mounts | awk '{print $2}')
log_info()
{
	[ $VERBOSE -ne 0 ] && echo "[INFO] $1"
}
log_pass()
{
	[ $VERBOSE -ne 0 ] && echo "$1 [PASS]"
	exit 0
}
log_fail()
{
	[ $VERBOSE -ne 0 ] && echo "$1 [FAIL]"
	exit 1
}
log_skip()
{
	[ $VERBOSE -ne 0 ] && echo "$1"
	exit 4
}
get_efivarfs_secureboot_mode()
{
	local efivarfs="/sys/firmware/efi/efivars"
	local secure_boot_file=""
	local setup_mode_file=""
	local secureboot_mode=0
	local setup_mode=0
	if ! grep -q "^\S\+ $efivarfs efivarfs" /proc/mounts; then
		log_info "efivars is not mounted on $efivarfs"
		return 0;
	fi
	secure_boot_file=$(find "$efivarfs" -name SecureBoot-* 2>/dev/null)
	setup_mode_file=$(find "$efivarfs" -name SetupMode-* 2>/dev/null)
	if [ -f "$secure_boot_file" ] && [ -f "$setup_mode_file" ]; then
		secureboot_mode=$(hexdump -v -e '/1 "%d\ "' \
			"$secure_boot_file"|cut -d' ' -f 5)
		setup_mode=$(hexdump -v -e '/1 "%d\ "' \
			"$setup_mode_file"|cut -d' ' -f 5)
		if [ $secureboot_mode -eq 1 ] && [ $setup_mode -eq 0 ]; then
			log_info "secure boot mode enabled (CONFIG_EFIVAR_FS)"
			return 1;
		fi
	fi
	return 0;
}
get_ppc64_secureboot_mode()
{
	local secure_boot_file="/proc/device-tree/ibm,secureboot/os-secureboot-enforcing"
	if [ -f $secure_boot_file ]; then
		log_info "Secureboot is enabled (Device tree)"
		return 1;
	fi
	log_info "Secureboot is not enabled (Device tree)"
	return 0;
}
get_arch()
{
	echo $(arch)
}
get_secureboot_mode()
{
	local secureboot_mode=0
	local system_arch=$(get_arch)
	if [ "$system_arch" == "ppc64le" ]; then
		get_ppc64_secureboot_mode
		secureboot_mode=$?
	else
		get_efivarfs_secureboot_mode
		secureboot_mode=$?
	fi
	if [ $secureboot_mode -eq 0 ]; then
		log_info "secure boot mode not enabled"
	fi
	return $secureboot_mode;
}
require_root_privileges()
{
	if [ $(id -ru) -ne 0 ]; then
		log_skip "requires root privileges"
	fi
}
kconfig_enabled()
{
	local config="$1"
	local msg="$2"
	grep -E -q $config $IKCONFIG
	if [ $? -eq 0 ]; then
		log_info "$msg"
		return 1
	fi
	return 0
}
get_kconfig()
{
	local proc_config="/proc/config.gz"
	local module_dir="/lib/modules/`uname -r`"
	local configs_module="$module_dir/kernel/kernel/configs.ko*"
	if [ -f $module_dir/config ]; then
		IKCONFIG=$module_dir/config
		return 1
	fi
	if [ ! -f $proc_config ]; then
		modprobe configs > /dev/null 2>&1
	fi
	if [ -f $proc_config ]; then
		cat $proc_config | gunzip > $IKCONFIG 2>/dev/null
		if [ $? -eq 0 ]; then
			return 1
		fi
	fi
	local extract_ikconfig="$module_dir/source/scripts/extract-ikconfig"
	if [ ! -f $extract_ikconfig ]; then
		log_skip "extract-ikconfig not found"
	fi
	$extract_ikconfig $KERNEL_IMAGE > $IKCONFIG 2>/dev/null
	if [ $? -eq 1 ]; then
		if [ ! -f $configs_module ]; then
			log_skip "CONFIG_IKCONFIG not enabled"
		fi
		$extract_ikconfig $configs_module > $IKCONFIG
		if [ $? -eq 1 ]; then
			log_skip "CONFIG_IKCONFIG not enabled"
		fi
	fi
	return 1
}
mount_securityfs()
{
	if [ -z $SECURITYFS ]; then
		SECURITYFS=/sys/kernel/security
		mount -t securityfs security $SECURITYFS
	fi
	if [ ! -d "$SECURITYFS" ]; then
		log_fail "$SECURITYFS :securityfs is not mounted"
	fi
}
check_ima_policy()
{
	local action="$1"
	local keypair1="$2"
	local keypair2="$3"
	local ret=0
	mount_securityfs
	local ima_policy=$SECURITYFS/ima/policy
	if [ ! -e $ima_policy ]; then
		log_fail "$ima_policy not found"
	fi
	if [ -n $keypair2 ]; then
		grep -e "^$action.*$keypair1" "$ima_policy" | \
			grep -q -e "$keypair2"
	else
		grep -q -e "^$action.*$keypair1" "$ima_policy"
	fi
	[ $? -eq 0 ] && ret=1
	return $ret
}
