check_rate()
{
	local rate=$1; shift
	local min=$1; shift
	local what=$1; shift
	if ((rate > min)); then
		return 0
	fi
	echo "$what $(humanize $ir) < $(humanize $min)" > /dev/stderr
	return 1
}
measure_rate()
{
	local sw_in=$1; shift   
	local host_in=$1; shift 
	local counter=$1; shift 
	local what=$1; shift
	local interval=10
	local i
	local ret=0
	local min_ingress=2147483648
	for i in {5..0}; do
		local t0=$(ethtool_stats_get $host_in $counter)
		local u0=$(ethtool_stats_get $sw_in $counter)
		sleep $interval
		local t1=$(ethtool_stats_get $host_in $counter)
		local u1=$(ethtool_stats_get $sw_in $counter)
		local ir=$(rate $u0 $u1 $interval)
		local er=$(rate $t0 $t1 $interval)
		if check_rate $ir $min_ingress "$what ingress rate"; then
			break
		fi
		if ((i == 0)); then
			ret=1
		fi
	done
	echo $ir $er
	return $ret
}
