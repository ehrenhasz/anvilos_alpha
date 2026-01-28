if [ $
	[ $
	asm_header_dir=tools/include/uapi/asm-generic
	arch_header_dir=tools/arch/${hostarch}/include/uapi/asm
else
	asm_header_dir=$1
	arch_header_dir=$2
fi
common_mman=${asm_header_dir}/mman-common.h
arch_mman=${arch_header_dir}/mman.h
prefix="PROT"
printf "static const char *mmap_prot[] = {\n"
regex=`printf '^[[:space:]]*
( ! test -f ${arch_mman} \
|| grep -E -q '
(grep -E $regex ${common_mman} | \
	grep -E -vw PROT_NONE | \
	sed -r "s/$regex/\2 \1 \1 \1 \2/g"	| \
	xargs printf "\t[ilog2(%s) + 1] = \"%s\",\n#ifndef ${prefix}_%s\n#define ${prefix}_%s %s\n#endif\n")
test -f ${arch_mman} && grep -E -q $regex ${arch_mman} &&
(grep -E $regex ${arch_mman} | \
	grep -E -vw PROT_NONE | \
	sed -r "s/$regex/\2 \1 \1 \1 \2/g"	| \
	xargs printf "\t[ilog2(%s) + 1] = \"%s\",\n#ifndef ${prefix}_%s\n#define ${prefix}_%s %s\n#endif\n")
printf "};\n"
