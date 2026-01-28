set -e
for file in "${KBUILD_IMAGE}" System.map
do
	if [ ! -f "${file}" ]; then
		echo >&2
		echo >&2 " *** Missing file: ${file}"
		echo >&2 ' *** You need to run "make" before "make install".'
		echo >&2
		exit 1
	fi
done
for file in "${HOME}/bin/${INSTALLKERNEL}"		\
	    "/sbin/${INSTALLKERNEL}"			\
	    "${srctree}/arch/${SRCARCH}/install.sh"	\
	    "${srctree}/arch/${SRCARCH}/boot/install.sh"
do
	if [ ! -x "${file}" ]; then
		continue
	fi
	exec "${file}" "${KERNELRELEASE}" "${KBUILD_IMAGE}" System.map "${INSTALL_PATH}"
done
echo "No install script found" >&2
exit 1
