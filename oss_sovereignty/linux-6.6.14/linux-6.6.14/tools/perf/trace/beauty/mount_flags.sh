[ $
printf "static const char *mount_flags[] = {\n"
regex='^[[:space:]]*
grep -E $regex ${header_dir}/mount.h | grep -E -v '(MSK|VERBOSE|MGC_VAL)\>' | \
	sed -r "s/$regex/\2 \2 \1/g" | sort -n | \
	xargs printf "\t[%s ? (ilog2(%s) + 1) : 0] = \"%s\",\n"
regex='^[[:space:]]*
grep -E $regex ${header_dir}/mount.h | \
	sed -r "s/$regex/\2 \1/g" | \
	xargs printf "\t[%s + 1] = \"%s\",\n"
printf "};\n"
