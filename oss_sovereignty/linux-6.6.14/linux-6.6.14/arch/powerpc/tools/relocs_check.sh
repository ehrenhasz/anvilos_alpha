if [ $
	echo "$0 [path to objdump] [path to nm] [path to vmlinux]" 1>&2
	exit 1
fi
bad_relocs=$(
${srctree}/scripts/relocs_check.sh "$@" |
	grep -F -w -v 'R_PPC64_RELATIVE
R_PPC64_NONE
R_PPC64_UADDR64
R_PPC_ADDR16_LO
R_PPC_ADDR16_HI
R_PPC_ADDR16_HA
R_PPC_RELATIVE
R_PPC_NONE'
)
if [ -z "$bad_relocs" ]; then
	exit 0
fi
num_bad=$(echo "$bad_relocs" | wc -l)
echo "WARNING: $num_bad bad relocations"
echo "$bad_relocs"
