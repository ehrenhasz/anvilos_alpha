source _debugfs_common.sh
dmesg -C
for file in "$DBGFS/"*
do
	(echo "$(basename "$f")" > "$DBGFS/rm_contexts") &> /dev/null
	if dmesg | grep -q BUG
	then
		dmesg
		exit 1
	fi
done
