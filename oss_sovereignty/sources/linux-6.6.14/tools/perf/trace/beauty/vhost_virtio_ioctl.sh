[ $
printf "static const char *vhost_virtio_ioctl_cmds[] = {\n"
regex='^
grep -E $regex ${header_dir}/vhost.h | \
	sed -r "s/$regex/\2 \1/g"	| \
	sort | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n"
printf "static const char *vhost_virtio_ioctl_read_cmds[] = {\n"
regex='^
grep -E $regex ${header_dir}/vhost.h | \
	sed -r "s/$regex/\2 \1/g"	| \
	sort | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n"
