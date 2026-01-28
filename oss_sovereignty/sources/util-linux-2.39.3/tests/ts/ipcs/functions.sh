test -f /proc/sys/kernel/shmall || ts_skip "no /proc"
PAGE_SIZE=$($TS_HELPER_SYSINFO pagesize)
IPCS_PROCFILES=(
	/proc/sys/kernel/shmmni
	/proc/sys/kernel/shmall
	/proc/sys/kernel/shmmax
)
IPCS_KERNEL_CMD=(
	"cat /proc/sys/kernel/shmmni"
	"echo \$(cat /proc/sys/kernel/shmall) / 1024 \* $PAGE_SIZE | bc -l | sed 's/\..*//'"
	"echo \$(cat /proc/sys/kernel/shmmax) / 1024 | bc -l | sed 's/\..*//'"
)
IPCS_CMD=(
	"$TS_CMD_IPCS -m -l | awk '/max number of segments/ { print \$6 }'"
	"$TS_CMD_IPCS -m -l | awk '/max total shared memory/ { print \$7 }'"
	"$TS_CMD_IPCS -m -l | awk '/max seg size/ { print \$6 }'"
)
IPCS_LIMITS=(
	32768
	$($TS_HELPER_SYSINFO ULONG_MAX32)
	$($TS_HELPER_SYSINFO ULONG_MAX32)
)
IPCS_IDX=$(seq 0 $(( ${#IPCS_PROCFILES[*]} - 1 )))
UINT64_MAX=$($TS_HELPER_SYSINFO UINT64_MAX)
function ipcs_limits_check {
	for i in $IPCS_IDX; do
		echo -n ${IPCS_PROCFILES[$i]}
		a=$(eval ${IPCS_KERNEL_CMD[$i]})
		b=$(eval ${IPCS_CMD[$i]})
		max_kbytes=$(bc <<< "$UINT64_MAX - ($UINT64_MAX % ($PAGE_SIZE / 1024))")
		if [ $(bc <<<"$a > $max_kbytes") -eq 1 ]; then
			a=$max_kbytes
		fi
		if [ x"$a" == x"$b" ]; then
			echo " OK"
		else
			echo " kernel=$a, ipcs=$b"
		fi
	done
}
ipcmk_output_handler() {
	awk -v text=$1 -v num=$2 '
	function isnum(x) {
		return(x == x + 0)
	}
	{
		if (isnum($NF)) {
			print $NF >> num
			$NF="<was_number>"
		}
		print $0 >> text
	}'
}
