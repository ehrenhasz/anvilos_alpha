CHECK_TC="yes"
TC_HIT_TIMEOUT=${TC_HIT_TIMEOUT:=1000} 
tc_check_packets()
{
	local id=$1
	local handle=$2
	local count=$3
	busywait "$TC_HIT_TIMEOUT" until_counter_is "== $count" \
		 tc_rule_handle_stats_get "$id" "$handle" > /dev/null
}
tc_check_at_least_x_packets()
{
	local id=$1
	local handle=$2
	local count=$3
	busywait "$TC_HIT_TIMEOUT" until_counter_is ">= $count" \
		 tc_rule_handle_stats_get "$id" "$handle" > /dev/null
}
tc_check_packets_hitting()
{
	local id=$1
	local handle=$2
	busywait "$TC_HIT_TIMEOUT" until_counter_is "> 0" \
		 tc_rule_handle_stats_get "$id" "$handle" > /dev/null
}
