tenths=date\ +%s%1N
wait_for_threads()
{
	tm_out=$3 ; [ -n "${tm_out}" ] || tm_out=50
	start_time=$($tenths)
	while [ -e "/proc/$1/task" ] ; do
		th_cnt=$(find "/proc/$1/task" -mindepth 1 -maxdepth 1 -printf x | wc -c)
		if [ "${th_cnt}" -ge "$2" ] ; then
			return 0
		fi
		if [ $(($($tenths) - start_time)) -ge $tm_out ] ; then
			echo "PID $1 does not have $2 threads"
			return 1
		fi
	done
	return 1
}
wait_for_perf_to_start()
{
	tm_out=$3 ; [ -n "${tm_out}" ] || tm_out=50
	echo "Waiting for \"perf record has started\" message"
	start_time=$($tenths)
	while [ -e "/proc/$1" ] ; do
		if grep -q "perf record has started" "$2" ; then
			echo OK
			break
		fi
		if [ $(($($tenths) - start_time)) -ge $tm_out ] ; then
			echo "perf recording did not start"
			return 1
		fi
	done
	return 0
}
wait_for_process_to_exit()
{
	tm_out=$2 ; [ -n "${tm_out}" ] || tm_out=50
	start_time=$($tenths)
	while [ -e "/proc/$1" ] ; do
		if [ $(($($tenths) - start_time)) -ge $tm_out ] ; then
			echo "PID $1 did not exit as expected"
			return 1
		fi
	done
	return 0
}
is_running()
{
	tm_out=$2 ; [ -n "${tm_out}" ] || tm_out=3
	start_time=$($tenths)
	while [ -e "/proc/$1" ] ; do
		if [ $(($($tenths) - start_time)) -gt $tm_out ] ; then
			return 0
		fi
	done
	echo "PID $1 exited prematurely"
	return 1
}
