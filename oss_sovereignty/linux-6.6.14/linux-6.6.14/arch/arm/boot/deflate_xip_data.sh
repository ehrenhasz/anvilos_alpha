set -e
VMLINUX="$1"
XIPIMAGE="$2"
DD="dd status=none"
case "$KBUILD_VERBOSE" in
*1*)
	set -x
	;;
esac
sym_val() {
	local val=$($NM "$VMLINUX" 2>/dev/null | sed -n "/ $1\$/{s/ .*$//p;q}")
	[ "$val" ] || { echo "can't find $1 in $VMLINUX" 1>&2; exit 1; }
	echo $((0x$val))
}
__data_loc=$(sym_val __data_loc)
_edata_loc=$(sym_val _edata_loc)
base_offset=$(sym_val _xiprom)
data_start=$(($__data_loc - $base_offset))
data_end=$(($_edata_loc - $base_offset))
file_end=$(${CONFIG_SHELL} "${srctree}/scripts/file-size.sh" "$XIPIMAGE")
if [ "$file_end" != "$data_end" ]; then
	printf "end of xipImage doesn't match with _edata_loc (%#x vs %#x)\n" \
	       $(($file_end + $base_offset)) $_edata_loc 1>&2
	exit 1;
fi
trap 'rm -f "$XIPIMAGE.tmp"; exit 1' 1 2 3
$DD if="$XIPIMAGE" count=$data_start iflag=count_bytes of="$XIPIMAGE.tmp"
$DD if="$XIPIMAGE"  skip=$data_start iflag=skip_bytes |
$KGZIP -9 >> "$XIPIMAGE.tmp"
mv -f "$XIPIMAGE.tmp" "$XIPIMAGE"
