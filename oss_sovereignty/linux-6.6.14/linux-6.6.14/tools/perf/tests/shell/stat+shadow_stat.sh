set -e
perf stat -a true > /dev/null 2>&1 || exit 2
perf stat -a -e cycles sleep 1 2>&1 | grep -e cpu_core && exit 2
test_global_aggr()
{
	perf stat -a --no-big-num -e cycles,instructions sleep 1  2>&1 | \
	grep -e cycles -e instructions | \
	while read num evt _hash ipc rest
	do
		if [ "$num" = "<not" ]; then
			continue
		fi
		if [ "$evt" = "cycles" ]; then
			cyc=$num
			continue
		fi
		if [ -z "$cyc" ]; then
			continue
		fi
		res=`printf "%.2f" "$(echo "scale=6; $num / $cyc" | bc -q)"`
		if [ "$ipc" != "$res" ]; then
			echo "IPC is different: $res != $ipc  ($num / $cyc)"
			exit 1
		fi
	done
}
test_no_aggr()
{
	perf stat -a -A --no-big-num -e cycles,instructions sleep 1  2>&1 | \
	grep ^CPU | \
	while read cpu num evt _hash ipc rest
	do
		if [ "$num" = "<not" ]; then
			continue
		fi
		if [ "$evt" = "cycles" ]; then
			results="$results $cpu:$num"
			continue
		fi
		cyc=${results
		cyc=${cyc%% *}
		if [ -z "$cyc" ]; then
			continue
		fi
		res=`printf "%.2f" "$(echo "scale=6; $num / $cyc" | bc -q)"`
		if [ "$ipc" != "$res" ]; then
			echo "IPC is different for $cpu: $res != $ipc  ($num / $cyc)"
			exit 1
		fi
	done
}
test_global_aggr
test_no_aggr
exit 0
