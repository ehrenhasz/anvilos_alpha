[ $
printf "static const char *sndrv_pcm_ioctl_cmds[] = {\n"
grep "^
	sed -r 's/^
printf "};\n"
