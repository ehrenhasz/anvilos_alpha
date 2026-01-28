[ $
printf "static const char *usbdevfs_ioctl_cmds[] = {\n"
regex="^#[[:space:]]*define[[:space:]]+USBDEVFS_(\w+)(\(\w+\))?[[:space:]]+_IO[CWR]{0,2}\([[:space:]]*(_IOC_\w+,[[:space:]]*)?'U'[[:space:]]*,[[:space:]]*([[:digit:]]+).*"
grep -E "$regex" ${header_dir}/usbdevice_fs.h | grep -E -v 'USBDEVFS_\w+32[[:space:]]' | \
	sed -r "s/$regex/\4 \1/g"	| \
	sort | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n\n"
printf "#if 0\n"
printf "static const char *usbdevfs_ioctl_32_cmds[] = {\n"
regex="^#[[:space:]]*define[[:space:]]+USBDEVFS_(\w+)[[:space:]]+_IO[WR]{0,2}\([[:space:]]*'U'[[:space:]]*,[[:space:]]*([[:digit:]]+).*"
grep -E $regex ${header_dir}/usbdevice_fs.h | grep -E 'USBDEVFS_\w+32[[:space:]]' | \
	sed -r "s/$regex/\2 \1/g"	| \
	sort | xargs printf "\t[%s] = \"%s\",\n"
printf "};\n"
printf "#endif\n"
