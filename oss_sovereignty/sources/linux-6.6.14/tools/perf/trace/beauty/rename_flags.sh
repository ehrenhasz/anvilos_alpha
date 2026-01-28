[ $
fs_header=${header_dir}/fs.h
printf "static const char *rename_flags[] = {\n"
regex='^[[:space:]]*
grep -E -q $regex ${fs_header} && \
(grep -E $regex ${fs_header} | \
	sed -r "s/$regex/\2 \1/g"	| \
	xargs printf "\t[%d + 1] = \"%s\",\n")
printf "};\n"
