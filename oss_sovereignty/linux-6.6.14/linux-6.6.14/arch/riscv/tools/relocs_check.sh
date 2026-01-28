if [ $
        echo "$0 [path to objdump] [path to nm] [path to vmlinux]" 1>&2
        exit 1
fi
bad_relocs=$(
${srctree}/scripts/relocs_check.sh "$@" |
	grep -F -w -v 'R_RISCV_RELATIVE'
)
if [ -z "$bad_relocs" ]; then
	exit 0
fi
num_bad=$(echo "$bad_relocs" | wc -l)
echo "WARNING: $num_bad bad relocations"
echo "$bad_relocs"
