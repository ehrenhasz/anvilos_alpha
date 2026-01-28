set -eu
TDIR="/usr/share/zfs/zfs-tests/tests/functional"
echo -n "TODO="
case "$1" in
  part1)
    echo "cli_root"
    ;;
  part2)
    ls $TDIR|grep '^[a-m]'|grep -v "cli_root"|xargs|tr -s ' ' ','
    ;;
  part3)
    ls $TDIR|grep '^[n-qs-z]'|xargs|tr -s ' ' ','
    ;;
  part4)
    ls $TDIR|grep '^r'|xargs|tr -s ' ' ','
    ;;
esac
