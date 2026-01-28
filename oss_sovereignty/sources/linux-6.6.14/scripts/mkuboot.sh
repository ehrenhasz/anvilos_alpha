MKIMAGE=$(type -path "${CROSS_COMPILE}mkimage")
if [ -z "${MKIMAGE}" ]; then
	MKIMAGE=$(type -path mkimage)
	if [ -z "${MKIMAGE}" ]; then
		echo '"mkimage" command not found - U-Boot images will not be built' >&2
		exit 1;
	fi
fi
${MKIMAGE} "$@"
