[ $
printf "static const char *socket_families[] = {\n"
regex='^
grep -E $regex ${header_dir}/socket.h | \
	sed -r "s/$regex/\2 \1/g"	| \
	xargs printf "\t[%s] = \"%s\",\n" | \
	grep -E -v "\"(UNIX|MAX)\""
printf "};\n"
