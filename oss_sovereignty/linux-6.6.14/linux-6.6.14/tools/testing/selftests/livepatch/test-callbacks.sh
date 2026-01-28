. $(dirname $0)/functions.sh
MOD_LIVEPATCH=test_klp_callbacks_demo
MOD_LIVEPATCH2=test_klp_callbacks_demo2
MOD_TARGET=test_klp_callbacks_mod
MOD_TARGET_BUSY=test_klp_callbacks_busy
setup_config
start_test "target module before livepatch"
load_mod $MOD_TARGET
load_lp $MOD_LIVEPATCH
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH
unload_mod $MOD_TARGET
check_result "% modprobe $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_init
% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
$MOD_LIVEPATCH: pre_patch_callback: $MOD_TARGET -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': starting patching transition
livepatch: '$MOD_LIVEPATCH': completing patching transition
$MOD_LIVEPATCH: post_patch_callback: vmlinux
$MOD_LIVEPATCH: post_patch_callback: $MOD_TARGET -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': patching complete
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH/enabled
livepatch: '$MOD_LIVEPATCH': initializing unpatching transition
$MOD_LIVEPATCH: pre_unpatch_callback: vmlinux
$MOD_LIVEPATCH: pre_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': starting unpatching transition
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
$MOD_LIVEPATCH: post_unpatch_callback: vmlinux
$MOD_LIVEPATCH: post_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': unpatching complete
% rmmod $MOD_LIVEPATCH
% rmmod $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_exit"
start_test "module_coming notifier"
load_lp $MOD_LIVEPATCH
load_mod $MOD_TARGET
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH
unload_mod $MOD_TARGET
check_result "% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting patching transition
livepatch: '$MOD_LIVEPATCH': completing patching transition
$MOD_LIVEPATCH: post_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': patching complete
% modprobe $MOD_TARGET
livepatch: applying patch '$MOD_LIVEPATCH' to loading module '$MOD_TARGET'
$MOD_LIVEPATCH: pre_patch_callback: $MOD_TARGET -> [MODULE_STATE_COMING] Full formed, running module_init
$MOD_LIVEPATCH: post_patch_callback: $MOD_TARGET -> [MODULE_STATE_COMING] Full formed, running module_init
$MOD_TARGET: ${MOD_TARGET}_init
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH/enabled
livepatch: '$MOD_LIVEPATCH': initializing unpatching transition
$MOD_LIVEPATCH: pre_unpatch_callback: vmlinux
$MOD_LIVEPATCH: pre_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': starting unpatching transition
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
$MOD_LIVEPATCH: post_unpatch_callback: vmlinux
$MOD_LIVEPATCH: post_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': unpatching complete
% rmmod $MOD_LIVEPATCH
% rmmod $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_exit"
start_test "module_going notifier"
load_mod $MOD_TARGET
load_lp $MOD_LIVEPATCH
unload_mod $MOD_TARGET
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH
check_result "% modprobe $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_init
% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
$MOD_LIVEPATCH: pre_patch_callback: $MOD_TARGET -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': starting patching transition
livepatch: '$MOD_LIVEPATCH': completing patching transition
$MOD_LIVEPATCH: post_patch_callback: vmlinux
$MOD_LIVEPATCH: post_patch_callback: $MOD_TARGET -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': patching complete
% rmmod $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_exit
$MOD_LIVEPATCH: pre_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_GOING] Going away
livepatch: reverting patch '$MOD_LIVEPATCH' on unloading module '$MOD_TARGET'
$MOD_LIVEPATCH: post_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_GOING] Going away
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH/enabled
livepatch: '$MOD_LIVEPATCH': initializing unpatching transition
$MOD_LIVEPATCH: pre_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting unpatching transition
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
$MOD_LIVEPATCH: post_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': unpatching complete
% rmmod $MOD_LIVEPATCH"
start_test "module_coming and module_going notifiers"
load_lp $MOD_LIVEPATCH
load_mod $MOD_TARGET
unload_mod $MOD_TARGET
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH
check_result "% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting patching transition
livepatch: '$MOD_LIVEPATCH': completing patching transition
$MOD_LIVEPATCH: post_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': patching complete
% modprobe $MOD_TARGET
livepatch: applying patch '$MOD_LIVEPATCH' to loading module '$MOD_TARGET'
$MOD_LIVEPATCH: pre_patch_callback: $MOD_TARGET -> [MODULE_STATE_COMING] Full formed, running module_init
$MOD_LIVEPATCH: post_patch_callback: $MOD_TARGET -> [MODULE_STATE_COMING] Full formed, running module_init
$MOD_TARGET: ${MOD_TARGET}_init
% rmmod $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_exit
$MOD_LIVEPATCH: pre_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_GOING] Going away
livepatch: reverting patch '$MOD_LIVEPATCH' on unloading module '$MOD_TARGET'
$MOD_LIVEPATCH: post_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_GOING] Going away
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH/enabled
livepatch: '$MOD_LIVEPATCH': initializing unpatching transition
$MOD_LIVEPATCH: pre_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting unpatching transition
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
$MOD_LIVEPATCH: post_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': unpatching complete
% rmmod $MOD_LIVEPATCH"
start_test "target module not present"
load_lp $MOD_LIVEPATCH
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH
check_result "% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting patching transition
livepatch: '$MOD_LIVEPATCH': completing patching transition
$MOD_LIVEPATCH: post_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': patching complete
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH/enabled
livepatch: '$MOD_LIVEPATCH': initializing unpatching transition
$MOD_LIVEPATCH: pre_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting unpatching transition
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
$MOD_LIVEPATCH: post_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': unpatching complete
% rmmod $MOD_LIVEPATCH"
start_test "pre-patch callback -ENODEV"
load_mod $MOD_TARGET
load_failing_mod $MOD_LIVEPATCH pre_patch_ret=-19
unload_mod $MOD_TARGET
check_result "% modprobe $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_init
% modprobe $MOD_LIVEPATCH pre_patch_ret=-19
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
test_klp_callbacks_demo: pre_patch_callback: vmlinux
livepatch: pre-patch callback failed for object 'vmlinux'
livepatch: failed to enable patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': canceling patching transition, going to unpatch
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
livepatch: '$MOD_LIVEPATCH': unpatching complete
modprobe: ERROR: could not insert '$MOD_LIVEPATCH': No such device
% rmmod $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_exit"
start_test "module_coming + pre-patch callback -ENODEV"
load_lp $MOD_LIVEPATCH
set_pre_patch_ret $MOD_LIVEPATCH -19
load_failing_mod $MOD_TARGET
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH
check_result "% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting patching transition
livepatch: '$MOD_LIVEPATCH': completing patching transition
$MOD_LIVEPATCH: post_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': patching complete
% echo -19 > /sys/module/$MOD_LIVEPATCH/parameters/pre_patch_ret
% modprobe $MOD_TARGET
livepatch: applying patch '$MOD_LIVEPATCH' to loading module '$MOD_TARGET'
$MOD_LIVEPATCH: pre_patch_callback: $MOD_TARGET -> [MODULE_STATE_COMING] Full formed, running module_init
livepatch: pre-patch callback failed for object '$MOD_TARGET'
livepatch: patch '$MOD_LIVEPATCH' failed for module '$MOD_TARGET', refusing to load module '$MOD_TARGET'
modprobe: ERROR: could not insert '$MOD_TARGET': No such device
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH/enabled
livepatch: '$MOD_LIVEPATCH': initializing unpatching transition
$MOD_LIVEPATCH: pre_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting unpatching transition
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
$MOD_LIVEPATCH: post_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': unpatching complete
% rmmod $MOD_LIVEPATCH"
start_test "multiple target modules"
load_mod $MOD_TARGET_BUSY block_transition=N
load_lp $MOD_LIVEPATCH
load_mod $MOD_TARGET
unload_mod $MOD_TARGET
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH
unload_mod $MOD_TARGET_BUSY
check_result "% modprobe $MOD_TARGET_BUSY block_transition=N
$MOD_TARGET_BUSY: ${MOD_TARGET_BUSY}_init
$MOD_TARGET_BUSY: busymod_work_func enter
$MOD_TARGET_BUSY: busymod_work_func exit
% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
$MOD_LIVEPATCH: pre_patch_callback: $MOD_TARGET_BUSY -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': starting patching transition
livepatch: '$MOD_LIVEPATCH': completing patching transition
$MOD_LIVEPATCH: post_patch_callback: vmlinux
$MOD_LIVEPATCH: post_patch_callback: $MOD_TARGET_BUSY -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': patching complete
% modprobe $MOD_TARGET
livepatch: applying patch '$MOD_LIVEPATCH' to loading module '$MOD_TARGET'
$MOD_LIVEPATCH: pre_patch_callback: $MOD_TARGET -> [MODULE_STATE_COMING] Full formed, running module_init
$MOD_LIVEPATCH: post_patch_callback: $MOD_TARGET -> [MODULE_STATE_COMING] Full formed, running module_init
$MOD_TARGET: ${MOD_TARGET}_init
% rmmod $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_exit
$MOD_LIVEPATCH: pre_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_GOING] Going away
livepatch: reverting patch '$MOD_LIVEPATCH' on unloading module '$MOD_TARGET'
$MOD_LIVEPATCH: post_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_GOING] Going away
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH/enabled
livepatch: '$MOD_LIVEPATCH': initializing unpatching transition
$MOD_LIVEPATCH: pre_unpatch_callback: vmlinux
$MOD_LIVEPATCH: pre_unpatch_callback: $MOD_TARGET_BUSY -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': starting unpatching transition
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
$MOD_LIVEPATCH: post_unpatch_callback: vmlinux
$MOD_LIVEPATCH: post_unpatch_callback: $MOD_TARGET_BUSY -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': unpatching complete
% rmmod $MOD_LIVEPATCH
% rmmod $MOD_TARGET_BUSY
$MOD_TARGET_BUSY: ${MOD_TARGET_BUSY}_exit"
start_test "busy target module"
load_mod $MOD_TARGET_BUSY block_transition=Y
load_lp_nowait $MOD_LIVEPATCH
loop_until 'grep -q '^1$' /sys/kernel/livepatch/$MOD_LIVEPATCH/transition' ||
	die "failed to stall transition"
load_mod $MOD_TARGET
unload_mod $MOD_TARGET
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH
unload_mod $MOD_TARGET_BUSY
check_result "% modprobe $MOD_TARGET_BUSY block_transition=Y
$MOD_TARGET_BUSY: ${MOD_TARGET_BUSY}_init
$MOD_TARGET_BUSY: busymod_work_func enter
% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
$MOD_LIVEPATCH: pre_patch_callback: $MOD_TARGET_BUSY -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': starting patching transition
% modprobe $MOD_TARGET
livepatch: applying patch '$MOD_LIVEPATCH' to loading module '$MOD_TARGET'
$MOD_LIVEPATCH: pre_patch_callback: $MOD_TARGET -> [MODULE_STATE_COMING] Full formed, running module_init
$MOD_TARGET: ${MOD_TARGET}_init
% rmmod $MOD_TARGET
$MOD_TARGET: ${MOD_TARGET}_exit
livepatch: reverting patch '$MOD_LIVEPATCH' on unloading module '$MOD_TARGET'
$MOD_LIVEPATCH: post_unpatch_callback: $MOD_TARGET -> [MODULE_STATE_GOING] Going away
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH/enabled
livepatch: '$MOD_LIVEPATCH': reversing transition from patching to unpatching
livepatch: '$MOD_LIVEPATCH': starting unpatching transition
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
$MOD_LIVEPATCH: post_unpatch_callback: vmlinux
$MOD_LIVEPATCH: post_unpatch_callback: $MOD_TARGET_BUSY -> [MODULE_STATE_LIVE] Normal state
livepatch: '$MOD_LIVEPATCH': unpatching complete
% rmmod $MOD_LIVEPATCH
% rmmod $MOD_TARGET_BUSY
$MOD_TARGET_BUSY: busymod_work_func exit
$MOD_TARGET_BUSY: ${MOD_TARGET_BUSY}_exit"
start_test "multiple livepatches"
load_lp $MOD_LIVEPATCH
load_lp $MOD_LIVEPATCH2
disable_lp $MOD_LIVEPATCH2
disable_lp $MOD_LIVEPATCH
unload_lp $MOD_LIVEPATCH2
unload_lp $MOD_LIVEPATCH
check_result "% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting patching transition
livepatch: '$MOD_LIVEPATCH': completing patching transition
$MOD_LIVEPATCH: post_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': patching complete
% modprobe $MOD_LIVEPATCH2
livepatch: enabling patch '$MOD_LIVEPATCH2'
livepatch: '$MOD_LIVEPATCH2': initializing patching transition
$MOD_LIVEPATCH2: pre_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH2': starting patching transition
livepatch: '$MOD_LIVEPATCH2': completing patching transition
$MOD_LIVEPATCH2: post_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH2': patching complete
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH2/enabled
livepatch: '$MOD_LIVEPATCH2': initializing unpatching transition
$MOD_LIVEPATCH2: pre_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH2': starting unpatching transition
livepatch: '$MOD_LIVEPATCH2': completing unpatching transition
$MOD_LIVEPATCH2: post_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH2': unpatching complete
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH/enabled
livepatch: '$MOD_LIVEPATCH': initializing unpatching transition
$MOD_LIVEPATCH: pre_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting unpatching transition
livepatch: '$MOD_LIVEPATCH': completing unpatching transition
$MOD_LIVEPATCH: post_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': unpatching complete
% rmmod $MOD_LIVEPATCH2
% rmmod $MOD_LIVEPATCH"
start_test "atomic replace"
load_lp $MOD_LIVEPATCH
load_lp $MOD_LIVEPATCH2 replace=1
disable_lp $MOD_LIVEPATCH2
unload_lp $MOD_LIVEPATCH2
unload_lp $MOD_LIVEPATCH
check_result "% modprobe $MOD_LIVEPATCH
livepatch: enabling patch '$MOD_LIVEPATCH'
livepatch: '$MOD_LIVEPATCH': initializing patching transition
$MOD_LIVEPATCH: pre_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': starting patching transition
livepatch: '$MOD_LIVEPATCH': completing patching transition
$MOD_LIVEPATCH: post_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH': patching complete
% modprobe $MOD_LIVEPATCH2 replace=1
livepatch: enabling patch '$MOD_LIVEPATCH2'
livepatch: '$MOD_LIVEPATCH2': initializing patching transition
$MOD_LIVEPATCH2: pre_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH2': starting patching transition
livepatch: '$MOD_LIVEPATCH2': completing patching transition
$MOD_LIVEPATCH2: post_patch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH2': patching complete
% echo 0 > /sys/kernel/livepatch/$MOD_LIVEPATCH2/enabled
livepatch: '$MOD_LIVEPATCH2': initializing unpatching transition
$MOD_LIVEPATCH2: pre_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH2': starting unpatching transition
livepatch: '$MOD_LIVEPATCH2': completing unpatching transition
$MOD_LIVEPATCH2: post_unpatch_callback: vmlinux
livepatch: '$MOD_LIVEPATCH2': unpatching complete
% rmmod $MOD_LIVEPATCH2
% rmmod $MOD_LIVEPATCH"
exit 0
