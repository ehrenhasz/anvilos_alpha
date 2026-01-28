[ $
printf "static const char *sndrv_pcm_ioctl_cmds[] = {\n"
grep "^#define[\t ]\+SNDRV_PCM_IOCTL_" $header_dir/asound.h | \
	sed -r 's/^
printf "};\n"
