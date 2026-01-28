[ $
printf "static const char *sndrv_ctl_ioctl_cmds[] = {\n"
grep "^
	sed -r 's/^
printf "};\n"
