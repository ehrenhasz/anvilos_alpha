[ $
printf "static const char *sndrv_ctl_ioctl_cmds[] = {\n"
grep "^#define[\t ]\+SNDRV_CTL_IOCTL_" $header_dir/asound.h | \
	sed -r 's/^
printf "};\n"
