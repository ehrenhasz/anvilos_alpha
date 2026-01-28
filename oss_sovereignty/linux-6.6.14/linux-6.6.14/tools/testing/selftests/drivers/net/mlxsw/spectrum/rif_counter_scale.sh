source ../rif_counter_scale.sh
rif_counter_get_target()
{
	local should_fail=$1; shift
	local max_cnts
	local max_rifs
	local target
	max_rifs=$(devlink_resource_size_get rifs)
	max_cnts=$(devlink_resource_size_get counters rif)
	((max_rifs -= $(devlink_resource_occ_get rifs)))
	((max_cnts /= 20))
	if ((max_cnts > max_rifs && should_fail)); then
		echo 0
		return
	fi
	target=$((max_rifs < max_cnts ? max_rifs : max_cnts))
	if ((! should_fail)); then
		echo $target
	else
		echo $((target + 1))
	fi
}
