if [ $
	[ $
	linux_header_dir=tools/include/uapi/linux
	header_dir=tools/include/uapi/asm-generic
	arch_header_dir=tools/arch/${hostarch}/include/uapi/asm
else
	linux_header_dir=$1
	header_dir=$2
	arch_header_dir=$3
fi
linux_mman=${linux_header_dir}/mman.h
arch_mman=${arch_header_dir}/mman.h
printf "static const char *mmap_flags[] = {\n"
regex='^[[:space:]]*
test -f ${arch_mman} && \
grep -E -q $regex ${arch_mman} && \
(grep -E $regex ${arch_mman} | \
	sed -r "s/$regex/\2 \1 \1 \1 \2/g"	| \
	xargs printf "\t[ilog2(%s) + 1] = \"%s\",\n#ifndef MAP_%s\n#define MAP_%s %s\n#endif\n")
grep -E -q $regex ${linux_mman} && \
(grep -E $regex ${linux_mman} | \
	grep -E -vw 'MAP_(UNINITIALIZED|TYPE|SHARED_VALIDATE)' | \
	sed -r "s/$regex/\2 \1 \1 \1 \2/g" | \
	xargs printf "\t[ilog2(%s) + 1] = \"%s\",\n#ifndef MAP_%s\n#define MAP_%s %s\n#endif\n")
( ! test -f ${arch_mman} || \
grep -E -q '
(grep -E $regex ${header_dir}/mman-common.h | \
	grep -E -vw 'MAP_(UNINITIALIZED|TYPE|SHARED_VALIDATE)' | \
	sed -r "s/$regex/\2 \1 \1 \1 \2/g"	| \
	xargs printf "\t[ilog2(%s) + 1] = \"%s\",\n#ifndef MAP_%s\n#define MAP_%s %s\n#endif\n")
( ! test -f ${arch_mman} || \
grep -E -q '
(grep -E $regex ${header_dir}/mman.h | \
	sed -r "s/$regex/\2 \1 \1 \1 \2/g"	| \
	xargs printf "\t[ilog2(%s) + 1] = \"%s\",\n#ifndef MAP_%s\n#define MAP_%s %s\n#endif\n")
printf "};\n"
