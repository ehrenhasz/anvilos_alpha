has_renamed_memintrinsics()
{
	grep -q "^CONFIG_KASAN=y$" ${KCONFIG_CONFIG} && \
		! grep -q "^CONFIG_CC_HAS_KASAN_MEMINTRINSIC_PREFIX=y" ${KCONFIG_CONFIG}
}
if has_renamed_memintrinsics
then
	MEM_FUNCS="__memcpy __memset"
else
	MEM_FUNCS="memcpy memset"
fi
WHITELIST="add_reloc_offset __bss_start __bss_stop copy_and_flush
_end enter_prom $MEM_FUNCS reloc_offset __secondary_hold
__secondary_hold_acknowledge __secondary_hold_spinloop __start
logo_linux_clut224 btext_prepare_BAT
reloc_got2 kernstart_addr memstart_addr linux_banner _stext
btext_setup_display TOC. relocate"
NM="$1"
OBJ="$2"
ERROR=0
check_section()
{
    file=$1
    section=$2
    size=$(objdump -h -j $section $file 2>/dev/null | awk "\$2 == \"$section\" {print \$3}")
    size=${size:-0}
    if [ $size -ne 0 ]; then
	ERROR=1
	echo "Error: Section $section not empty in prom_init.c" >&2
    fi
}
for UNDEF in $($NM -u $OBJ | awk '{print $2}')
do
	UNDEF="${UNDEF#.}"
	case "$KBUILD_VERBOSE" in
	*1*)
		echo "Checking prom_init.o symbol '$UNDEF'" ;;
	esac
	OK=0
	for WHITE in $WHITELIST
	do
		if [ "$UNDEF" = "$WHITE" ]; then
			OK=1
			break
		fi
	done
	case $UNDEF in
	_restgpr_*|_restgpr0_*|_rest32gpr_*)
		OK=1
		;;
	_savegpr_*|_savegpr0_*|_save32gpr_*)
		OK=1
		;;
	esac
	if [ $OK -eq 0 ]; then
		ERROR=1
		echo "Error: External symbol '$UNDEF' referenced" \
		     "from prom_init.c" >&2
	fi
done
check_section $OBJ .data
check_section $OBJ .bss
check_section $OBJ .init.data
exit $ERROR
