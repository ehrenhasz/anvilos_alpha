[ $# -eq 1 ] && header_dir=$1 || header_dir=tools/include/uapi/linux/
printf "static const char *fadvise_advices[] = {\n"
regex='^[[:space:]]*#[[:space:]]*define[[:space:]]+POSIX_FADV_(\w+)[[:space:]]+([[:digit:]]+)[[:space:]]+.*'
grep -E $regex ${header_dir}/fadvise.h | \
	sed -r "s/$regex/\2 \1/g"	| \
	sort | xargs printf "\t[%s] = \"%s\",\n" | \
	grep -v "[6].*DONTNEED" | grep -v "[7].*NOREUSE"
printf "};\n"
