set -eu
dist=no
distdir=.
while getopts D: flag
do
	case $flag in
		\?) echo "Usage: $0 [-D distdir] [file]" >&2; exit 1;;
		D)  dist=yes; distdir=${OPTARG};;
		*)  ;;
	esac
done
shift $((OPTIND - 1))
top_srcdir="$(dirname "$0")/.."
GITREV="${1:-include/zfs_gitrev.h}"
case "${GITREV}" in
	/*) echo "Error: ${GITREV} should be a relative path" >&2
	    exit 1;;
	*) ;;
esac
ZFS_GITREV=$({ cd "${top_srcdir}" &&
	git describe --always --long --dirty 2>/dev/null; } || :)
if [ -z "${ZFS_GITREV}" ]
then
	if [ -f "${top_srcdir}/${GITREV}" ]
	then
		ZFS_GITREV=$(sed -n \
			'1s/^#define[[:blank:]]ZFS_META_GITREV "\([^"]*\)"$/\1/p' \
			"${top_srcdir}/${GITREV}")
	fi
elif [ "${dist}" = yes ]
then
	ZFS_GITREV="${ZFS_GITREV}-dist"
fi
ZFS_GITREV=${ZFS_GITREV:-unknown}
GITREVTMP="${GITREV}~"
printf '#define\tZFS_META_GITREV "%s"\n' "${ZFS_GITREV}" >"${GITREVTMP}"
GITREV="${distdir}/${GITREV}"
if cmp -s "${GITREV}" "${GITREVTMP}"
then
	rm -f "${GITREVTMP}"
else
	mv -f "${GITREVTMP}" "${GITREV}"
fi
