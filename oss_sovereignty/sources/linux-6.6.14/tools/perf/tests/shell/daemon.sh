check_line_first()
{
	local line=$1
	local name=$2
	local base=$3
	local output=$4
	local lock=$5
	local up=$6
	local line_name
	line_name=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $2 }'`
	local line_base
	line_base=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $3 }'`
	local line_output
	line_output=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $4 }'`
	local line_lock
	line_lock=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $5 }'`
	local line_up
	line_up=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $6 }'`
	if [ "${name}" != "${line_name}" ]; then
		echo "FAILED: wrong name"
		error=1
	fi
	if [ "${base}" != "${line_base}" ]; then
		echo "FAILED: wrong base"
		error=1
	fi
	if [ "${output}" != "${line_output}" ]; then
		echo "FAILED: wrong output"
		error=1
	fi
	if [ "${lock}" != "${line_lock}" ]; then
		echo "FAILED: wrong lock"
		error=1
	fi
	if [ "${up}" != "${line_up}" ]; then
		echo "FAILED: wrong up"
		error=1
	fi
}
check_line_other()
{
	local line=$1
	local name=$2
	local run=$3
	local base=$4
	local output=$5
	local control=$6
	local ack=$7
	local up=$8
	local line_name
	line_name=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $2 }'`
	local line_run
	line_run=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $3 }'`
	local line_base
	line_base=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $4 }'`
	local line_output
	line_output=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $5 }'`
	local line_control
	line_control=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $6 }'`
	local line_ack
	line_ack=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $7 }'`
	local line_up
	line_up=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $8 }'`
	if [ "${name}" != "${line_name}" ]; then
		echo "FAILED: wrong name"
		error=1
	fi
	if [ "${run}" != "${line_run}" ]; then
		echo "FAILED: wrong run"
		error=1
	fi
	if [ "${base}" != "${line_base}" ]; then
		echo "FAILED: wrong base"
		error=1
	fi
	if [ "${output}" != "${line_output}" ]; then
		echo "FAILED: wrong output"
		error=1
	fi
	if [ "${control}" != "${line_control}" ]; then
		echo "FAILED: wrong control"
		error=1
	fi
	if [ "${ack}" != "${line_ack}" ]; then
		echo "FAILED: wrong ack"
		error=1
	fi
	if [ "${up}" != "${line_up}" ]; then
		echo "FAILED: wrong up"
		error=1
	fi
}
daemon_exit()
{
	local config=$1
	local line
	line=`perf daemon --config ${config} -x: | head -1`
	local pid
	pid=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $1 }'`
	trap - SIGINT SIGTERM
	perf daemon stop --config ${config}
	tail --pid=${pid} -f /dev/null
}
daemon_start()
{
	local config=$1
	local session=$2
	perf daemon start --config ${config}
	trap 'echo "FAILED: Signal caught"; daemon_exit "${config}"; exit 1' SIGINT SIGTERM
	local state="FAIL"
	local retries=0
	while [ "${state}" != "OK" ]; do
		state=`perf daemon ping --config ${config} --session ${session} | awk '{ print $1 }'`
		sleep 0.05
		retries=$((${retries} +1))
		if [ ${retries} -ge 600 ]; then
			echo "FAILED: Timeout waiting for daemon to ping"
			daemon_exit ${config}
			exit 1
		fi
	done
}
test_list()
{
	echo "test daemon list"
	local config
	config=$(mktemp /tmp/perf.daemon.config.XXX)
	local base
	base=$(mktemp -d /tmp/perf.daemon.base.XXX)
	cat <<EOF > ${config}
[daemon]
base=BASE
[session-size]
run = -e cpu-clock -m 1 sleep 10
[session-time]
run = -e task-clock -m 1 sleep 10
EOF
	sed -i -e "s|BASE|${base}|" ${config}
	daemon_start ${config} size
	local line
	line=`perf daemon --config ${config} -x: | head -1`
	check_line_first ${line} daemon ${base} ${base}/output ${base}/lock "0"
	local line
	line=`perf daemon --config ${config} -x: | head -2 | tail -1`
	check_line_other "${line}" size "-e cpu-clock -m 1 sleep 10" ${base}/session-size \
			 ${base}/session-size/output ${base}/session-size/control \
			 ${base}/session-size/ack "0"
	local line
	line=`perf daemon --config ${config} -x: | head -3 | tail -1`
	check_line_other "${line}" time "-e task-clock -m 1 sleep 10" ${base}/session-time \
			 ${base}/session-time/output ${base}/session-time/control \
			 ${base}/session-time/ack "0"
	daemon_exit ${config}
	rm -rf ${base}
	rm -f ${config}
}
test_reconfig()
{
	echo "test daemon reconfig"
	local config
	config=$(mktemp /tmp/perf.daemon.config.XXX)
	local base
	base=$(mktemp -d /tmp/perf.daemon.base.XXX)
	cat <<EOF > ${config}
[daemon]
base=BASE
[session-size]
run = -e cpu-clock -m 1 sleep 10
[session-time]
run = -e task-clock -m 1 sleep 10
EOF
	sed -i -e "s|BASE|${base}|" ${config}
	daemon_start ${config} size
	local line
	line=`perf daemon --config ${config} -x: | head -3 | tail -1`
	check_line_other "${line}" time "-e task-clock -m 1 sleep 10" ${base}/session-time \
			 ${base}/session-time/output ${base}/session-time/control ${base}/session-time/ack "0"
	local pid
	pid=`echo "${line}" | awk 'BEGIN { FS = ":" } ; { print $1 }'`
	local config_new=${config}.new
	cat <<EOF > ${config_new}
[daemon]
base=BASE
[session-size]
run = -e cpu-clock -m 1 sleep 10
[session-time]
run = -e cpu-clock -m 1 sleep 10
EOF
	sed -i -e "s|BASE|${base}|" ${config_new}
	cp ${config_new} ${config}
	tail --pid=${pid} -f /dev/null
	local state="FAIL"
	while [ "${state}" != "OK" ]; do
		state=`perf daemon ping --config ${config} --session time | awk '{ print $1 }'`
	done
	local line
	line=`perf daemon --config ${config} -x: | head -3 | tail -1`
	check_line_other "${line}" time "-e cpu-clock -m 1 sleep 10" ${base}/session-time \
			 ${base}/session-time/output ${base}/session-time/control ${base}/session-time/ack "0"
	local config_empty=${config}.empty
	cat <<EOF > ${config_empty}
[daemon]
base=BASE
EOF
	sed -i -e "s|BASE|${base}|" ${config_empty}
	cp ${config_empty} ${config}
	local state="OK"
	while [ "${state}" != "FAIL" ]; do
		state=`perf daemon ping --config ${config} --session time | awk '{ print $1 }'`
	done
	local state="OK"
	while [ "${state}" != "FAIL" ]; do
		state=`perf daemon ping --config ${config} --session size | awk '{ print $1 }'`
	done
	local one
	one=`perf daemon --config ${config} -x: | wc -l`
	if [ ${one} -ne "1" ]; then
		echo "FAILED: wrong list output"
		error=1
	fi
	cp ${config_new} ${config}
	local state="FAIL"
	while [ "${state}" != "OK" ]; do
		state=`perf daemon ping --config ${config} --session size | awk '{ print $1 }'`
	done
	local state="FAIL"
	while [ "${state}" != "OK" ]; do
		state=`perf daemon ping --config ${config} --session time | awk '{ print $1 }'`
	done
	daemon_exit ${config}
	rm -rf ${base}
	rm -f ${config}
	rm -f ${config_new}
	rm -f ${config_empty}
}
test_stop()
{
	echo "test daemon stop"
	local config
	config=$(mktemp /tmp/perf.daemon.config.XXX)
	local base
	base=$(mktemp -d /tmp/perf.daemon.base.XXX)
	cat <<EOF > ${config}
[daemon]
base=BASE
[session-size]
run = -e cpu-clock -m 1 sleep 10
[session-time]
run = -e task-clock -m 1 sleep 10
EOF
	sed -i -e "s|BASE|${base}|" ${config}
	daemon_start ${config} size
	local pid_size
	pid_size=`perf daemon --config ${config} -x: | head -2 | tail -1 |
		  awk 'BEGIN { FS = ":" } ; { print $1 }'`
	local pid_time
	pid_time=`perf daemon --config ${config} -x: | head -3 | tail -1 |
		  awk 'BEGIN { FS = ":" } ; { print $1 }'`
	if [ ! -d "/proc/${pid_size}" ]; then
		echo "FAILED: session size not up"
	fi
	if [ ! -d "/proc/${pid_time}" ]; then
		echo "FAILED: session time not up"
	fi
	daemon_exit ${config}
	if [ -d "/proc/${pid_size}" ]; then
		echo "FAILED: session size still up"
	fi
	if [ -d "/proc/${pid_time}" ]; then
		echo "FAILED: session time still up"
	fi
	rm -rf ${base}
	rm -f ${config}
}
test_signal()
{
	echo "test daemon signal"
	local config
	config=$(mktemp /tmp/perf.daemon.config.XXX)
	local base
	base=$(mktemp -d /tmp/perf.daemon.base.XXX)
	cat <<EOF > ${config}
[daemon]
base=BASE
[session-test]
run = -e cpu-clock --switch-output -m 1 sleep 10
EOF
	sed -i -e "s|BASE|${base}|" ${config}
	daemon_start ${config} test
	perf daemon signal --config ${config} --session test
	perf daemon signal --config ${config}
	daemon_exit ${config}
	count=`ls ${base}/session-test/*perf.data* | wc -l`
	if [ ${count} -ne 3 ]; then
		error=1
		echo "FAILED: perf data no generated"
	fi
	rm -rf ${base}
	rm -f ${config}
}
test_ping()
{
	echo "test daemon ping"
	local config
	config=$(mktemp /tmp/perf.daemon.config.XXX)
	local base
	base=$(mktemp -d /tmp/perf.daemon.base.XXX)
	cat <<EOF > ${config}
[daemon]
base=BASE
[session-size]
run = -e cpu-clock -m 1 sleep 10
[session-time]
run = -e task-clock -m 1 sleep 10
EOF
	sed -i -e "s|BASE|${base}|" ${config}
	daemon_start ${config} size
	size=`perf daemon ping --config ${config} --session size | awk '{ print $1 }'`
	type=`perf daemon ping --config ${config} --session time | awk '{ print $1 }'`
	if [ ${size} != "OK" ] || [ ${type} != "OK" ]; then
		error=1
		echo "FAILED: daemon ping failed"
	fi
	daemon_exit ${config}
	rm -rf ${base}
	rm -f ${config}
}
test_lock()
{
	echo "test daemon lock"
	local config
	config=$(mktemp /tmp/perf.daemon.config.XXX)
	local base
	base=$(mktemp -d /tmp/perf.daemon.base.XXX)
	cat <<EOF > ${config}
[daemon]
base=BASE
[session-size]
run = -e cpu-clock -m 1 sleep 10
EOF
	sed -i -e "s|BASE|${base}|" ${config}
	daemon_start ${config} size
	failed=`perf daemon start --config ${config} 2>&1 | awk '{ print $1 }'`
	if [ ${failed} != "failed:" ]; then
		error=1
		echo "FAILED: daemon lock failed"
	fi
	daemon_exit ${config}
	rm -rf ${base}
	rm -f ${config}
}
error=0
test_list
test_reconfig
test_stop
test_signal
test_ping
test_lock
exit ${error}
