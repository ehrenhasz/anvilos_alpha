set -e
format="%8s\t%8s\t%8s\n"
me=`basename $0`
sysd=${sysfs_dir:-/sys}
test ! -d "$sysd/block" && {
	echo "$me Error: sysfs is not mounted" 1>&2
	exit 1
}
for d in `ls -d $sysd/block/etherd* 2>/dev/null | grep -v p` end; do
	test $d = end && continue
	dev=`echo "$d" | sed 's/.*!//'`
	printf "$format" \
		"$dev" \
		"`cat \"$d/netif\"`" \
		"`cat \"$d/state\"`"
done | sort
