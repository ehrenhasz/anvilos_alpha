[ $# -eq 1 ] && header_dir=$1 || header_dir=tools/perf/trace/beauty/include/linux/
printf "static const char *socket_families[] = {\n"
regex='^#define[[:space:]]+AF_(\w+)[[:space:]]+([[:digit:]]+).*'
grep -E $regex ${header_dir}/socket.h | \
	sed -r "s/$regex/\2 \1/g"	| \
	xargs printf "\t[%s] = \"%s\",\n" | \
	grep -E -v "\"(UNIX|MAX)\""
printf "};\n"
