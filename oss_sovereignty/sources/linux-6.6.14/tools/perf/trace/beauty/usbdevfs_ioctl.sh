[ $
printf "static const char *usbdevfs_ioctl_cmds[] = {\n"
regex="^
grep -E "$regex" ${header_dir}/usbdevice_fs.h | grep -E -v 'USBDEVFS_\w+32[[:space:]]' | \
	sed -r "s/$regex/\4 \1/g"	| \
	sort | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n\n"
printf "
printf "static const char *usbdevfs_ioctl_32_cmds[] = {\n"
regex="^
grep -E $regex ${header_dir}/usbdevice_fs.h | grep -E 'USBDEVFS_\w+32[[:space:]]' | \
	sed -r "s/$regex/\2 \1/g"	| \
	sort | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n"
printf "
