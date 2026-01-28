TCID="zram02"
ERR_CODE=0
. ./zram_lib.sh
dev_num=1
zram_max_streams="2"
zram_sizes="1048576" 
zram_mem_limits="1M"
check_prereqs
zram_load
zram_max_streams
zram_set_disksizes
zram_set_memlimit
zram_makeswap
zram_swapoff
zram_cleanup
if [ $ERR_CODE -ne 0 ]; then
	echo "$TCID : [FAIL]"
else
	echo "$TCID : [PASS]"
fi
