SCRIPT_COMMON=${SCRIPT_COMMON:-${0%/*}/common.sh}
. "${SCRIPT_COMMON}" || exit
PROG=zfs-tests.sh
VERBOSE="no"
QUIET=""
CLEANUP="yes"
CLEANUPALL="no"
KMSG=""
LOOPBACK="yes"
STACK_TRACER="no"
FILESIZE="4G"
DEFAULT_RUNFILES="common.run,$(uname | tr '[:upper:]' '[:lower:]').run"
RUNFILES=${RUNFILES:-$DEFAULT_RUNFILES}
FILEDIR=${FILEDIR:-/var/tmp}
DISKS=${DISKS:-""}
SINGLETEST=""
SINGLETESTUSER="root"
TAGS=""
ITERATIONS=1
ZFS_DBGMSG="$STF_SUITE/callbacks/zfs_dbgmsg.ksh"
ZFS_DMESG="$STF_SUITE/callbacks/zfs_dmesg.ksh"
UNAME=$(uname)
RERUN=""
KMEMLEAK=""
if [ "$UNAME" = "FreeBSD" ] ; then
	TESTFAIL_CALLBACKS=${TESTFAIL_CALLBACKS:-"$ZFS_DMESG"}
	LOSETUP=/sbin/mdconfig
	DMSETUP=/sbin/gpart
else
	ZFS_MMP="$STF_SUITE/callbacks/zfs_mmp.ksh"
	TESTFAIL_CALLBACKS=${TESTFAIL_CALLBACKS:-"$ZFS_DBGMSG:$ZFS_DMESG:$ZFS_MMP"}
	LOSETUP=${LOSETUP:-/sbin/losetup}
	DMSETUP=${DMSETUP:-/sbin/dmsetup}
fi
msg() {
	if [ "$VERBOSE" = "yes" ]; then
		echo "$@"
	fi
}
fail() {
	echo "$PROG: $1" >&2
	cleanup
	exit 1
}
cleanup_freebsd_loopback() {
	for TEST_LOOPBACK in ${LOOPBACKS}; do
		if [ -c "/dev/${TEST_LOOPBACK}" ]; then
			sudo "${LOSETUP}" -d -u "${TEST_LOOPBACK}" ||
			    echo "Failed to destroy: ${TEST_LOOPBACK}"
		fi
	done
}
cleanup_linux_loopback() {
	for TEST_LOOPBACK in ${LOOPBACKS}; do
		LOOP_DEV="${TEST_LOOPBACK##*/}"
		DM_DEV=$(sudo "${DMSETUP}" ls 2>/dev/null | \
		    awk -v l="${LOOP_DEV}" '$0 ~ l {print $1}')
		if [ -n "$DM_DEV" ]; then
			sudo "${DMSETUP}" remove "${DM_DEV}" ||
			    echo "Failed to remove: ${DM_DEV}"
		fi
		if [ -n "${TEST_LOOPBACK}" ]; then
			sudo "${LOSETUP}" -d "${TEST_LOOPBACK}" ||
			    echo "Failed to remove: ${TEST_LOOPBACK}"
		fi
	done
}
cleanup() {
	if [ "$CLEANUP" = "no" ]; then
		return 0
	fi
	if [ "$LOOPBACK" = "yes" ]; then
		if [ "$UNAME" = "FreeBSD" ] ; then
			cleanup_freebsd_loopback
		else
			cleanup_linux_loopback
		fi
	fi
	rm -f ${FILES} >/dev/null 2>&1
	if [ "$STF_PATH_REMOVE" = "yes" ] && [ -d "$STF_PATH" ]; then
		rm -Rf "$STF_PATH"
	fi
}
trap cleanup EXIT
cleanup_all() {
	TEST_POOLS=$(ASAN_OPTIONS=detect_leaks=false "$ZPOOL" list -Ho name | grep testpool)
	if [ "$UNAME" = "FreeBSD" ] ; then
		TEST_LOOPBACKS=$(sudo "${LOSETUP}" -l)
	else
		TEST_LOOPBACKS=$("${LOSETUP}" -a | awk -F: '/file-vdev/ {print $1}')
	fi
	TEST_FILES=$(ls "${FILEDIR}"/file-vdev* /var/tmp/file-vdev* 2>/dev/null)
	msg
	msg "--- Cleanup ---"
	msg "Removing pool(s):     $(echo ${TEST_POOLS})"
	for TEST_POOL in $TEST_POOLS; do
		sudo env ASAN_OPTIONS=detect_leaks=false "$ZPOOL" destroy "${TEST_POOL}"
	done
	if [ "$UNAME" != "FreeBSD" ] ; then
		msg "Removing all dm(s):   $(sudo "${DMSETUP}" ls |
		    grep loop | tr '\n' ' ')"
		sudo "${DMSETUP}" remove_all
	fi
	msg "Removing loopback(s): $(echo ${TEST_LOOPBACKS})"
	for TEST_LOOPBACK in $TEST_LOOPBACKS; do
		if [ "$UNAME" = "FreeBSD" ] ; then
			sudo "${LOSETUP}" -d -u "${TEST_LOOPBACK}"
		else
			sudo "${LOSETUP}" -d "${TEST_LOOPBACK}"
		fi
	done
	msg "Removing files(s):    $(echo ${TEST_FILES})"
	sudo rm -f ${TEST_FILES}
}
find_runfile() {
	NAME=$1
	if [ -f "$RUNFILE_DIR/$NAME" ]; then
		echo "$RUNFILE_DIR/$NAME"
	elif [ -f "$RUNFILE_DIR/$NAME.run" ]; then
		echo "$RUNFILE_DIR/$NAME.run"
	elif [ -f "$NAME" ]; then
		echo "$NAME"
	elif [ -f "$NAME.run" ]; then
		echo "$NAME.run"
	else
		return 1
	fi
}
create_links() {
	dir_list="$1"
	file_list="$2"
	[ -n "$STF_PATH" ] || fail "STF_PATH wasn't correctly set"
	for i in $file_list; do
		for j in $dir_list; do
			[ ! -e "$STF_PATH/$i" ] || continue
			if [ ! -d "$j/$i" ] && [ -e "$j/$i" ]; then
				ln -sf "$j/$i" "$STF_PATH/$i" || \
				    fail "Couldn't link $i"
				break
			fi
		done
		[ ! -e "$STF_PATH/$i" ] && \
		    STF_MISSING_BIN="$STF_MISSING_BIN $i"
	done
	STF_MISSING_BIN=${STF_MISSING_BIN
}
constrain_path() {
	. "$STF_SUITE/include/commands.cfg"
	SYSTEM_DIRS="/usr/local/bin /usr/local/sbin"
	SYSTEM_DIRS="$SYSTEM_DIRS /usr/bin /usr/sbin /bin /sbin $LIBEXEC_DIR"
	if [ "$INTREE" = "yes" ]; then
		STF_PATH="$BIN_DIR"
		STF_PATH_REMOVE="no"
		STF_MISSING_BIN=""
		if [ ! -d "$STF_PATH" ]; then
			mkdir "$STF_PATH"
			chmod 755 "$STF_PATH" || fail "Couldn't chmod $STF_PATH"
		fi
		create_links "$CMD_DIR" "$ZFS_FILES"
		create_links "$CMD_DIR/tests/zfs-tests/cmd" "$ZFSTEST_FILES"
	else
		SYSTEMDIR=${SYSTEMDIR:-/var/tmp/constrained_path.XXXXXX}
		STF_PATH=$(mktemp -d "$SYSTEMDIR")
		STF_PATH_REMOVE="yes"
		STF_MISSING_BIN=""
		chmod 755 "$STF_PATH" || fail "Couldn't chmod $STF_PATH"
		create_links "$SYSTEM_DIRS" "$ZFS_FILES"
		create_links "$STF_SUITE/bin" "$ZFSTEST_FILES"
	fi
	SYSTEM_FILES="$SYSTEM_FILES_COMMON"
	if [ "$UNAME" = "FreeBSD" ] ; then
		SYSTEM_FILES="$SYSTEM_FILES $SYSTEM_FILES_FREEBSD"
	else
		SYSTEM_FILES="$SYSTEM_FILES $SYSTEM_FILES_LINUX"
	fi
	create_links "$SYSTEM_DIRS" "$SYSTEM_FILES"
	if [ "$UNAME" = "Linux" ] ; then
		ln -fs /sbin/fsck.ext4 "$STF_PATH/fsck"
		ln -fs /sbin/mkfs.ext4 "$STF_PATH/newfs"
		ln -fs "$STF_PATH/gzip" "$STF_PATH/compress"
		ln -fs "$STF_PATH/gunzip" "$STF_PATH/uncompress"
	elif [ "$UNAME" = "FreeBSD" ] ; then
		ln -fs /usr/local/bin/ksh93 "$STF_PATH/ksh"
	fi
}
usage() {
cat << EOF
USAGE:
$0 [-hvqxkfS] [-s SIZE] [-r RUNFILES] [-t PATH] [-u USER]
DESCRIPTION:
	ZFS Test Suite launch script
OPTIONS:
	-h          Show this message
	-v          Verbose zfs-tests.sh output
	-q          Quiet test-runner output
	-x          Remove all testpools, dm, lo, and files (unsafe)
	-k          Disable cleanup after test failure
	-K          Log test names to /dev/kmsg
	-f          Use files only, disables block device tests
	-S          Enable stack tracer (negative performance impact)
	-c          Only create and populate constrained path
	-R          Automatically rerun failing tests
	-m          Enable kmemleak reporting (Linux only)
	-n NFSFILE  Use the nfsfile to determine the NFS configuration
	-I NUM      Number of iterations
	-d DIR      Use world-writable DIR for files and loopback devices
	-s SIZE     Use vdevs of SIZE (default: 4G)
	-r RUNFILES Run tests in RUNFILES (default: ${DEFAULT_RUNFILES})
	-t PATH     Run single test at PATH relative to test suite
	-T TAGS     Comma separated list of tags (default: 'functional')
	-u USER     Run single test as USER (default: root)
EXAMPLES:
$0 -v
$0 -r linux-fast
$0 -t tests/functional/cli_root/zfs_bookmark/zfs_bookmark_cliargs.ksh
$0 -x
EOF
}
while getopts 'hvqxkKfScRmn:d:s:r:?t:T:u:I:' OPTION; do
	case $OPTION in
	h)
		usage
		exit 1
		;;
	v)
		VERBOSE="yes"
		;;
	q)
		QUIET="yes"
		;;
	x)
		CLEANUPALL="yes"
		;;
	k)
		CLEANUP="no"
		;;
	K)
		KMSG="yes"
		;;
	f)
		LOOPBACK="no"
		;;
	S)
		STACK_TRACER="yes"
		;;
	c)
		constrain_path
		exit
		;;
	R)
		RERUN="yes"
		;;
	m)
		KMEMLEAK="yes"
		;;
	n)
		nfsfile=$OPTARG
		[ -f "$nfsfile" ] || fail "Cannot read file: $nfsfile"
		export NFS=1
		. "$nfsfile"
		;;
	d)
		FILEDIR="$OPTARG"
		;;
	I)
		ITERATIONS="$OPTARG"
		if [ "$ITERATIONS" -le 0 ]; then
			fail "Iterations must be greater than 0."
		fi
		;;
	s)
		FILESIZE="$OPTARG"
		;;
	r)
		RUNFILES="$OPTARG"
		;;
	t)
		if [ -n "$SINGLETEST" ]; then
			fail "-t can only be provided once."
		fi
		SINGLETEST="$OPTARG"
		;;
	T)
		TAGS="$OPTARG"
		;;
	u)
		SINGLETESTUSER="$OPTARG"
		;;
	?)
		usage
		exit
		;;
	*)
		;;
	esac
done
shift $((OPTIND-1))
FILES=${FILES:-"$FILEDIR/file-vdev0 $FILEDIR/file-vdev1 $FILEDIR/file-vdev2"}
LOOPBACKS=${LOOPBACKS:-""}
if [ -n "$SINGLETEST" ]; then
	if [ -n "$TAGS" ]; then
		fail "-t and -T are mutually exclusive."
	fi
	RUNFILE_DIR="/var/tmp"
	RUNFILES="zfs-tests.$$.run"
	[ -n "$QUIET" ] && SINGLEQUIET="True" || SINGLEQUIET="False"
	cat >"${RUNFILE_DIR}/${RUNFILES}" << EOF
[DEFAULT]
pre =
quiet = $SINGLEQUIET
pre_user = root
user = $SINGLETESTUSER
timeout = 600
post_user = root
post =
outputdir = /var/tmp/test_results
EOF
	SINGLETESTDIR="${SINGLETEST%/*}"
	SETUPDIR="$SINGLETESTDIR"
	[ "${SETUPDIR#/}" = "$SETUPDIR" ] && SETUPDIR="$STF_SUITE/$SINGLETESTDIR"
	[ -x "$SETUPDIR/setup.ksh"   ] && SETUPSCRIPT="setup"     || SETUPSCRIPT=
	[ -x "$SETUPDIR/cleanup.ksh" ] && CLEANUPSCRIPT="cleanup" || CLEANUPSCRIPT=
	SINGLETESTFILE="${SINGLETEST##*/}"
	cat >>"${RUNFILE_DIR}/${RUNFILES}" << EOF
[$SINGLETESTDIR]
tests = ['$SINGLETESTFILE']
pre = $SETUPSCRIPT
post = $CLEANUPSCRIPT
tags = ['functional']
EOF
fi
TAGS=${TAGS:='functional'}
R=""
IFS=,
for RUNFILE in $RUNFILES; do
	if [ -n "$RUNFILE" ]; then
		SAVED_RUNFILE="$RUNFILE"
		RUNFILE=$(find_runfile "$RUNFILE") ||
			fail "Cannot find runfile: $SAVED_RUNFILE"
		R="$R,$RUNFILE"
	fi
	if [ ! -r "$RUNFILE" ]; then
		fail "Cannot read runfile: $RUNFILE"
	fi
done
unset IFS
RUNFILES=${R
if [ "$(id -u)" = "0" ]; then
	fail "This script must not be run as root."
fi
if [ "$(sudo id -un)" != "root" ]; then
	fail "Passwordless sudo access required."
fi
constrain_path
if [ "$UNAME" = "FreeBSD" ]; then
	sudo ln -fs /usr/local/bin/ksh93 /bin/ksh
fi
[ -e "$STF_PATH/ksh" ] || fail "This test suite requires ksh."
[ -e "$STF_SUITE/include/default.cfg" ] || fail \
    "Missing $STF_SUITE/include/default.cfg file."
if [ "$STACK_TRACER" = "yes" ]; then
	sudo "${ZFS_SH}" -S >/dev/null 2>&1
else
	sudo "${ZFS_SH}" >/dev/null 2>&1
fi
if [ "$CLEANUPALL" = "yes" ]; then
	cleanup_all
fi
if [ -z "${KEEP}" ]; then
	KEEP="$(ASAN_OPTIONS=detect_leaks=false "$ZPOOL" list -Ho name | tr -s '[:space:]' ' ')"
	if [ -z "${KEEP}" ]; then
		KEEP="rpool"
	fi
else
	KEEP="$(echo "$KEEP" | tr -s '[:space:]' ' ')"
fi
__ZFS_POOL_EXCLUDE="$KEEP"
. "$STF_SUITE/include/default.cfg"
if [ -z "${DISKS}" ]; then
	[ "$TAGS" = "perf" ] && fail "Running perf tests without disks."
	for TEST_FILE in ${FILES}; do
		[ -f "$TEST_FILE" ] && fail "Failed file exists: ${TEST_FILE}"
		truncate -s "${FILESIZE}" "${TEST_FILE}" ||
		    fail "Failed creating: ${TEST_FILE} ($?)"
	done
	if [ "$LOOPBACK" = "yes" ]; then
		test -x "$LOSETUP" || fail "$LOSETUP utility must be installed"
		for TEST_FILE in ${FILES}; do
			if [ "$UNAME" = "FreeBSD" ] ; then
				MDDEVICE=$(sudo "${LOSETUP}" -a -t vnode -f "${TEST_FILE}")
				if [ -z "$MDDEVICE" ] ; then
					fail "Failed: ${TEST_FILE} -> loopback"
				fi
				DISKS="$DISKS $MDDEVICE"
				LOOPBACKS="$LOOPBACKS $MDDEVICE"
			else
				TEST_LOOPBACK=$(sudo "${LOSETUP}" --show -f "${TEST_FILE}") ||
				    fail "Failed: ${TEST_FILE} -> ${TEST_LOOPBACK}"
				BASELOOPBACK="${TEST_LOOPBACK##*/}"
				DISKS="$DISKS $BASELOOPBACK"
				LOOPBACKS="$LOOPBACKS $TEST_LOOPBACK"
			fi
		done
		DISKS=${DISKS
		LOOPBACKS=${LOOPBACKS
	else
		DISKS="$FILES"
	fi
fi
NUM_DISKS=$(echo "${DISKS}" | awk '{print NF}')
if [ "$TAGS" != "perf" ]; then
	[ "$NUM_DISKS" -lt 3 ] && fail "Not enough disks ($NUM_DISKS/3 minimum)"
fi
if command -v setenforce >/dev/null; then
	sudo setenforce permissive >/dev/null 2>&1
fi
if [ -e /sys/module/zfs/parameters/zfs_dbgmsg_enable ]; then
	sudo sh -c "echo 1 >/sys/module/zfs/parameters/zfs_dbgmsg_enable"
	sudo sh -c "echo 0 >/proc/spl/kstat/zfs/dbgmsg"
fi
msg
msg "--- Configuration ---"
msg "Runfiles:        $RUNFILES"
msg "STF_TOOLS:       $STF_TOOLS"
msg "STF_SUITE:       $STF_SUITE"
msg "STF_PATH:        $STF_PATH"
msg "FILEDIR:         $FILEDIR"
msg "FILES:           $FILES"
msg "LOOPBACKS:       $LOOPBACKS"
msg "DISKS:           $DISKS"
msg "NUM_DISKS:       $NUM_DISKS"
msg "FILESIZE:        $FILESIZE"
msg "ITERATIONS:      $ITERATIONS"
msg "TAGS:            $TAGS"
msg "STACK_TRACER:    $STACK_TRACER"
msg "Keep pool(s):    $KEEP"
msg "Missing util(s): $STF_MISSING_BIN"
msg ""
export STF_TOOLS
export STF_SUITE
export STF_PATH
export DISKS
export FILEDIR
export KEEP
export __ZFS_POOL_EXCLUDE
export TESTFAIL_CALLBACKS
mktemp_file() {
	if [ "$UNAME" = "FreeBSD" ]; then
		mktemp -u "${FILEDIR}/$1.XXXXXX"
	else
		mktemp -ut "$1.XXXXXX" -p "$FILEDIR"
	fi
}
mkdir -p "$FILEDIR" || :
RESULTS_FILE=$(mktemp_file zts-results)
REPORT_FILE=$(mktemp_file zts-report)
msg "${TEST_RUNNER}" \
    "${QUIET:+-q}" \
    "${KMEMLEAK:+-m}" \
    "${KMSG:+-K}" \
    "-c \"${RUNFILES}\"" \
    "-T \"${TAGS}\"" \
    "-i \"${STF_SUITE}\"" \
    "-I \"${ITERATIONS}\""
{ PATH=$STF_PATH \
    ${TEST_RUNNER} \
    ${QUIET:+-q} \
    ${KMEMLEAK:+-m} \
    ${KMSG:+-K} \
    -c "${RUNFILES}" \
    -T "${TAGS}" \
    -i "${STF_SUITE}" \
    -I "${ITERATIONS}" \
    2>&1; echo $? >"$REPORT_FILE"; } | tee "$RESULTS_FILE"
read -r RUNRESULT <"$REPORT_FILE"
${ZTS_REPORT} ${RERUN:+--no-maybes} "$RESULTS_FILE" >"$REPORT_FILE"
RESULT=$?
if [ "$RESULT" -eq "2" ] && [ -n "$RERUN" ]; then
	MAYBES="$($ZTS_REPORT --list-maybes)"
	TEMP_RESULTS_FILE=$(mktemp_file zts-results-tmp)
	TEST_LIST=$(mktemp_file test-list)
	grep "^Test:.*\[FAIL\]" "$RESULTS_FILE" >"$TEMP_RESULTS_FILE"
	for test_name in $MAYBES; do
		grep "$test_name " "$TEMP_RESULTS_FILE" >>"$TEST_LIST"
	done
	{ PATH=$STF_PATH \
	    ${TEST_RUNNER} \
	        ${QUIET:+-q} \
	        ${KMEMLEAK:+-m} \
	    -c "${RUNFILES}" \
	    -T "${TAGS}" \
	    -i "${STF_SUITE}" \
	    -I "${ITERATIONS}" \
	    -l "${TEST_LIST}" \
	    2>&1; echo $? >"$REPORT_FILE"; } | tee "$RESULTS_FILE"
	read -r RUNRESULT <"$REPORT_FILE"
	${ZTS_REPORT} --no-maybes "$RESULTS_FILE" >"$REPORT_FILE"
	RESULT=$?
fi
cat "$REPORT_FILE"
RESULTS_DIR=$(awk '/^Log directory/ { print $3 }' "$RESULTS_FILE")
if [ -d "$RESULTS_DIR" ]; then
	cat "$RESULTS_FILE" "$REPORT_FILE" >"$RESULTS_DIR/results"
fi
rm -f "$RESULTS_FILE" "$REPORT_FILE" "$TEST_LIST" "$TEMP_RESULTS_FILE"
if [ -n "$SINGLETEST" ]; then
	rm -f "$RUNFILES" >/dev/null 2>&1
fi
[ "$RUNRESULT" -gt 3 ] && exit "$RUNRESULT" || exit "$RESULT"
