source _debugfs_common.sh
dmesg -C
for file in "$DBGFS/"*
do
	./huge_count_read_write "$file"
done
if dmesg | grep -q WARNING
then
	dmesg
	exit 1
else
	exit 0
fi
