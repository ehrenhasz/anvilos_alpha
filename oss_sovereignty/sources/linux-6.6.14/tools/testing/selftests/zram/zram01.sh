TCID="zram01"
ERR_CODE=0
. ./zram_lib.sh
dev_num=1
zram_max_streams="2"
zram_sizes="2097152" 
zram_mem_limits="2M"
zram_filesystems="ext4"
zram_algs="lzo"
zram_fill_fs()
{
	for i in $(seq $dev_start $dev_end); do
		echo "fill zram$i..."
		local b=0
		while [ true ]; do
			dd conv=notrunc if=/dev/zero of=zram${i}/file \
				oflag=append count=1 bs=1024 status=none \
				> /dev/null 2>&1 || break
			b=$(($b + 1))
		done
		echo "zram$i can be filled with '$b' KB"
		local mem_used_total=`awk '{print $3}' "/sys/block/zram$i/mm_stat"`
		local v=$((100 * 1024 * $b / $mem_used_total))
		if [ "$v" -lt 100 ]; then
			 echo "FAIL compression ratio: 0.$v:1"
			 ERR_CODE=-1
			 return
		fi
		echo "zram compression ratio: $(echo "scale=2; $v / 100 " | bc):1: OK"
	done
}
check_prereqs
zram_load
zram_max_streams
zram_compress_alg
zram_set_disksizes
zram_set_memlimit
zram_makefs
zram_mount
zram_fill_fs
zram_cleanup
if [ $ERR_CODE -ne 0 ]; then
	echo "$TCID : [FAIL]"
else
	echo "$TCID : [PASS]"
fi
