[ $
printf "#ifndef DRM_COMMAND_BASE\n"
grep "#define DRM_COMMAND_BASE" $header_dir/drm.h
printf "#endif\n"
printf "static const char *drm_ioctl_cmds[] = {\n"
grep "^#define DRM_IOCTL.*DRM_IO" $header_dir/drm.h | \
	sed -r 's/^
grep "^#define DRM_I915_[A-Z_0-9]\+[	 ]\+0x" $header_dir/i915_drm.h | \
	sed -r 's/^
printf "};\n"
