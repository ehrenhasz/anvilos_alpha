[ $
printf "static const char *prctl_options[] = {\n"
regex='^
grep -E $regex ${header_dir}/prctl.h | grep -v PR_SET_PTRACER | \
	sed -r "s/$regex/\2 \1/g"	| \
	sort -n | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n"
printf "static const char *prctl_set_mm_options[] = {\n"
regex='^
grep -E $regex ${header_dir}/prctl.h | \
	sed -r "s/$regex/\2 \1/g"	| \
	sort -n | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n"
