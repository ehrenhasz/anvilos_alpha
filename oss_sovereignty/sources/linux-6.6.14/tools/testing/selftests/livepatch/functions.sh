MAX_RETRIES=600
RETRY_INTERVAL=".1"	# seconds
KLP_SYSFS_DIR="/sys/kernel/livepatch"
ksft_skip=4
function log() {
	echo "$1" > /dev/kmsg
}
function skip() {
	log "SKIP: $1"
	echo "SKIP: $1" >&2
	exit $ksft_skip
}
function is_root() {
	uid=$(id -u)
	if [ $uid -ne 0 ]; then
		echo "skip all tests: must be run as root" >&2
		exit $ksft_skip
	fi
}
function die() {
	log "ERROR: $1"
	echo "ERROR: $1" >&2
	exit 1
}
function save_dmesg() {
	SAVED_DMESG=$(mktemp --tmpdir -t klp-dmesg-XXXXXX)
	dmesg > "$SAVED_DMESG"
}
function cleanup_dmesg_file() {
	rm -f "$SAVED_DMESG"
}
function push_config() {
	DYNAMIC_DEBUG=$(grep '^kernel/livepatch' /sys/kernel/debug/dynamic_debug/control | \
			awk -F'[: ]' '{print "file " $1 " line " $2 " " $4}')
	FTRACE_ENABLED=$(sysctl --values kernel.ftrace_enabled)
}
function pop_config() {
	if [[ -n "$DYNAMIC_DEBUG" ]]; then
		echo -n "$DYNAMIC_DEBUG" > /sys/kernel/debug/dynamic_debug/control
	fi
	if [[ -n "$FTRACE_ENABLED" ]]; then
		sysctl kernel.ftrace_enabled="$FTRACE_ENABLED" &> /dev/null
	fi
}
function set_dynamic_debug() {
        cat <<-EOF > /sys/kernel/debug/dynamic_debug/control
		file kernel/livepatch/* +p
		func klp_try_switch_task -p
		EOF
}
function set_ftrace_enabled() {
	local can_fail=0
	if [[ "$1" == "--fail" ]] ; then
		can_fail=1
		shift
	fi
	local err=$(sysctl -q kernel.ftrace_enabled="$1" 2>&1)
	local result=$(sysctl --values kernel.ftrace_enabled)
	if [[ "$result" != "$1" ]] ; then
		if [[ $can_fail -eq 1 ]] ; then
			echo "livepatch: $err" | sed 's#/proc/sys/kernel/#kernel.#' > /dev/kmsg
			return
		fi
		skip "failed to set kernel.ftrace_enabled = $1"
	fi
	echo "livepatch: kernel.ftrace_enabled = $result" > /dev/kmsg
}
function cleanup() {
	pop_config
	cleanup_dmesg_file
}
function setup_config() {
	is_root
	push_config
	set_dynamic_debug
	set_ftrace_enabled 1
	trap cleanup EXIT INT TERM HUP
}
function loop_until() {
	local cmd="$*"
	local i=0
	while true; do
		eval "$cmd" && return 0
		[[ $((i++)) -eq $MAX_RETRIES ]] && return 1
		sleep $RETRY_INTERVAL
	done
}
function assert_mod() {
	local mod="$1"
	modprobe --dry-run "$mod" &>/dev/null
}
function is_livepatch_mod() {
	local mod="$1"
	if [[ $(modinfo "$mod" | awk '/^livepatch:/{print $NF}') == "Y" ]]; then
		return 0
	fi
	return 1
}
function __load_mod() {
	local mod="$1"; shift
	local msg="% modprobe $mod $*"
	log "${msg%% }"
	ret=$(modprobe "$mod" "$@" 2>&1)
	if [[ "$ret" != "" ]]; then
		die "$ret"
	fi
	loop_until '[[ -e "/sys/module/$mod" ]]' ||
		die "failed to load module $mod"
}
function load_mod() {
	local mod="$1"; shift
	assert_mod "$mod" ||
		skip "unable to load module ${mod}, verify CONFIG_TEST_LIVEPATCH=m and run self-tests as root"
	is_livepatch_mod "$mod" &&
		die "use load_lp() to load the livepatch module $mod"
	__load_mod "$mod" "$@"
}
function load_lp_nowait() {
	local mod="$1"; shift
	assert_mod "$mod" ||
		skip "unable to load module ${mod}, verify CONFIG_TEST_LIVEPATCH=m and run self-tests as root"
	is_livepatch_mod "$mod" ||
		die "module $mod is not a livepatch"
	__load_mod "$mod" "$@"
	loop_until '[[ -e "/sys/kernel/livepatch/$mod" ]]' ||
		die "failed to load module $mod (sysfs)"
}
function load_lp() {
	local mod="$1"; shift
	load_lp_nowait "$mod" "$@"
	loop_until 'grep -q '^0$' /sys/kernel/livepatch/$mod/transition' ||
		die "failed to complete transition"
}
function load_failing_mod() {
	local mod="$1"; shift
	local msg="% modprobe $mod $*"
	log "${msg%% }"
	ret=$(modprobe "$mod" "$@" 2>&1)
	if [[ "$ret" == "" ]]; then
		die "$mod unexpectedly loaded"
	fi
	log "$ret"
}
function unload_mod() {
	local mod="$1"
	loop_until '[[ $(cat "/sys/module/$mod/refcnt") == "0" ]]' ||
		die "failed to unload module $mod (refcnt)"
	log "% rmmod $mod"
	ret=$(rmmod "$mod" 2>&1)
	if [[ "$ret" != "" ]]; then
		die "$ret"
	fi
	loop_until '[[ ! -e "/sys/module/$mod" ]]' ||
		die "failed to unload module $mod (/sys/module)"
}
function unload_lp() {
	unload_mod "$1"
}
function disable_lp() {
	local mod="$1"
	log "% echo 0 > /sys/kernel/livepatch/$mod/enabled"
	echo 0 > /sys/kernel/livepatch/"$mod"/enabled
	loop_until '[[ ! -e "/sys/kernel/livepatch/$mod" ]]' ||
		die "failed to disable livepatch $mod"
}
function set_pre_patch_ret {
	local mod="$1"; shift
	local ret="$1"
	log "% echo $ret > /sys/module/$mod/parameters/pre_patch_ret"
	echo "$ret" > /sys/module/"$mod"/parameters/pre_patch_ret
	loop_until '[[ $(cat "/sys/module/$mod/parameters/pre_patch_ret") == "$ret" ]]' ||
		die "failed to set pre_patch_ret parameter for $mod module"
}
function start_test {
	local test="$1"
	save_dmesg
	echo -n "TEST: $test ... "
	log "===== TEST: $test ====="
}
function check_result {
	local expect="$*"
	local result
	result=$(dmesg | comm --nocheck-order -13 "$SAVED_DMESG" - | \
		 grep -e 'livepatch:' -e 'test_klp' | \
		 grep -v '\(tainting\|taints\) kernel' | \
		 sed 's/^\[[ 0-9.]*\] //')
	if [[ "$expect" == "$result" ]] ; then
		echo "ok"
	else
		echo -e "not ok\n\n$(diff -upr --label expected --label result <(echo "$expect") <(echo "$result"))\n"
		die "livepatch kselftest(s) failed"
	fi
	cleanup_dmesg_file
}
function check_sysfs_rights() {
	local mod="$1"; shift
	local rel_path="$1"; shift
	local expected_rights="$1"; shift
	local path="$KLP_SYSFS_DIR/$mod/$rel_path"
	local rights=$(/bin/stat --format '%A' "$path")
	if test "$rights" != "$expected_rights" ; then
		die "Unexpected access rights of $path: $expected_rights vs. $rights"
	fi
}
function check_sysfs_value() {
	local mod="$1"; shift
	local rel_path="$1"; shift
	local expected_value="$1"; shift
	local path="$KLP_SYSFS_DIR/$mod/$rel_path"
	local value=`cat $path`
	if test "$value" != "$expected_value" ; then
		die "Unexpected value in $path: $expected_value vs. $value"
	fi
}
