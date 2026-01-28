set -e
sfile="$(readlink -f "$0")"
outdir="$(pwd)"
tarfile=$1
cpio_dir=$outdir/${tarfile%/*}/.tmp_cpio_dir
dir_list="
include/
arch/$SRCARCH/include/
"
type cpio > /dev/null
all_dirs=
if [ "$building_out_of_srctree" ]; then
	for d in $dir_list; do
		all_dirs="$all_dirs $srctree/$d"
	done
fi
all_dirs="$all_dirs $dir_list"
headers_md5="$(find $all_dirs -name "*.h"			|
		grep -v "include/generated/utsversion.h"	|
		grep -v "include/generated/autoconf.h"	|
		xargs ls -l | md5sum | cut -d ' ' -f1)"
this_file_md5="$(ls -l $sfile | md5sum | cut -d ' ' -f1)"
if [ -f $tarfile ]; then tarfile_md5="$(md5sum $tarfile | cut -d ' ' -f1)"; fi
if [ -f kernel/kheaders.md5 ] &&
	[ "$(head -n 1 kernel/kheaders.md5)" = "$headers_md5" ] &&
	[ "$(head -n 2 kernel/kheaders.md5 | tail -n 1)" = "$this_file_md5" ] &&
	[ "$(tail -n 1 kernel/kheaders.md5)" = "$tarfile_md5" ]; then
		exit
fi
echo "  GEN     $tarfile"
rm -rf $cpio_dir
mkdir $cpio_dir
if [ "$building_out_of_srctree" ]; then
	(
		cd $srctree
		for f in $dir_list
			do find "$f" -name "*.h";
		done | cpio --quiet -pd $cpio_dir
	)
fi
for f in $dir_list;
	do find "$f" -name "*.h";
done | cpio --quiet -pdu $cpio_dir >/dev/null 2>&1
find $cpio_dir -type f -print0 |
	xargs -0 -P8 -n1 perl -pi -e 'BEGIN {undef $/;}; s/\/\*((?!SPDX).)*?\*\///smg;'
tar "${KBUILD_BUILD_TIMESTAMP:+--mtime=$KBUILD_BUILD_TIMESTAMP}" \
    --owner=0 --group=0 --sort=name --numeric-owner \
    -I $XZ -cf $tarfile -C $cpio_dir/ . > /dev/null
echo $headers_md5 > kernel/kheaders.md5
echo "$this_file_md5" >> kernel/kheaders.md5
echo "$(md5sum $tarfile | cut -d ' ' -f1)" >> kernel/kheaders.md5
rm -rf $cpio_dir
