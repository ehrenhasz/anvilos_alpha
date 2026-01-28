[ $
printf "static const char *pkey_alloc_access_rights[] = {\n"
regex='^[[:space:]]*
grep -E $regex ${header_dir}/mman-common.h	| \
	sed -r "s/$regex/\2 \2 \1/g"	| \
	sort | xargs printf "\t[%s ? (ilog2(%s) + 1) : 0] = \"%s\",\n"
printf "};\n"
