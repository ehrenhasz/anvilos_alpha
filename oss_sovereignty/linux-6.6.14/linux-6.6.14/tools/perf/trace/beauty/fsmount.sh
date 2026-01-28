if [ $
	linux_header_dir=tools/include/uapi/linux
else
	linux_header_dir=$1
fi
linux_mount=${linux_header_dir}/mount.h
printf "static const char *fsmount_attr_flags[] = {\n"
regex='^[[:space:]]*
grep -E $regex ${linux_mount} | grep -v MOUNT_ATTR_RELATIME | \
	sed -r "s/$regex/\2 \1/g"	| \
	xargs printf "\t[ilog2(%s) + 1] = \"%s\",\n"
printf "};\n"
