test -d /proc || ts_skip "/proc not available"
function check_test_sigreceive {
	local rc=0
	local pid=$1
	for i in 0.01 0.1 1 1 1 1; do
		if [ ! -f /proc/$pid/status ]; then
			echo "kill_functions.sh: /proc/$pid/status: No such file or directory"
			sleep 2
			rc=1
			break
		fi
		sigmask=$((16#$( awk '/SigCgt/ { print $2}' /proc/$pid/status) ))
		if [ $(( $sigmask & 1 )) == 1 ]; then
			rc=1
			break
		fi
		sleep $i
	done
	return $rc
}
