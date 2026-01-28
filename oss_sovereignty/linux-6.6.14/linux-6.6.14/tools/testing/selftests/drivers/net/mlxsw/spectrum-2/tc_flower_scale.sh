source ../tc_flower_scale.sh
tc_flower_get_target()
{
	local should_fail=$1; shift
	local max_cnts
	max_cnts=$(devlink_resource_size_get counters flow)
	((max_cnts -= $(devlink_resource_occ_get counters flow)))
	((max_cnts /= 2))
	if ((! should_fail)); then
		echo $max_cnts
	else
		echo $((max_cnts + 1))
	fi
}
