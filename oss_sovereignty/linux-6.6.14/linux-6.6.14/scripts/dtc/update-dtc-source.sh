set -ev
DTC_UPSTREAM_PATH=`pwd`/../dtc
DTC_LINUX_PATH=`pwd`/scripts/dtc
DTC_SOURCE="checks.c data.c dtc.c dtc.h flattree.c fstree.c livetree.c srcpos.c \
		srcpos.h treesource.c util.c util.h version_gen.h \
		dtc-lexer.l dtc-parser.y"
LIBFDT_SOURCE="fdt.c fdt.h fdt_addresses.c fdt_empty_tree.c \
		fdt_overlay.c fdt_ro.c fdt_rw.c fdt_strerror.c fdt_sw.c \
		fdt_wip.c libfdt.h libfdt_env.h libfdt_internal.h"
FDTOVERLAY_SOURCE=fdtoverlay.c
get_last_dtc_version() {
	git log --oneline scripts/dtc/ | grep 'upstream' | head -1 | sed -e 's/^.* \(.*\)/\1/'
}
last_dtc_ver=$(get_last_dtc_version)
cd $DTC_UPSTREAM_PATH
make clean
make check
dtc_version=$(git describe HEAD)
dtc_log=$(git log --oneline ${last_dtc_ver}..)
cd $DTC_LINUX_PATH
for f in $DTC_SOURCE $FDTOVERLAY_SOURCE; do
	cp ${DTC_UPSTREAM_PATH}/${f} ${f}
	git add ${f}
done
for f in $LIBFDT_SOURCE; do
       cp ${DTC_UPSTREAM_PATH}/libfdt/${f} libfdt/${f}
       git add libfdt/${f}
done
sed -i -- 's/
sed -i -- 's/
git add ./libfdt/libfdt.h
commit_msg=$(cat << EOF
scripts/dtc: Update to upstream version ${dtc_version}
This adds the following commits from upstream:
${dtc_log}
EOF
)
git commit -e -v -s -m "${commit_msg}"
