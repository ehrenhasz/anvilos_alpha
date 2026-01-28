[ $
printf "static const char *perf_ioctl_cmds[] = {\n"
regex='^
grep -E $regex ${header_dir}/perf_event.h	| \
	sed -r "s/$regex/\2 \1/g"	| \
	sort | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n"
