[ $
printf "static const char *madvise_advices[] = {\n"
regex='^[[:space:]]*
grep -E $regex ${header_dir}/mman-common.h | \
	sed -r "s/$regex/\2 \1/g"	| \
	sort -n | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n"
