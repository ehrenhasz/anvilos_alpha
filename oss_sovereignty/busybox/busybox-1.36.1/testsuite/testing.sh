export FAILCOUNT=0
export SKIP=
test x"$ECHO" != x"" || {
	ECHO="echo"
	test x"`echo -ne`" = x"" || {
		ECHO="$PWD/echo-ne"
		test -x "$ECHO" || {
			gcc -Os -o "$ECHO" ../scripts/echo.c || exit 1
		}
	}
	export ECHO
}
optional()
{
	SKIP=
	while test "$1"; do
		case "${OPTIONFLAGS}" in
			*:$1:*) ;;
			*) SKIP=1; return ;;
		esac
		shift
	done
}
testing()
{
  NAME="$1"
  [ -n "$1" ] || NAME="$2"
  if [ $
  then
    echo "Test $NAME has wrong number of arguments: $# (must be 5)" >&2
    exit 1
  fi
  [ -z "$DEBUG" ] || set -x
  if [ -n "$SKIP" ]
  then
    echo "SKIPPED: $NAME"
    return 0
  fi
  $ECHO -ne "$3" > expected
  $ECHO -ne "$4" > input
  [ -z "$VERBOSE" ] || echo ======================
  [ -z "$VERBOSE" ] || echo "echo -ne '$4' >input"
  [ -z "$VERBOSE" ] || echo "echo -ne '$5' | $2"
  $ECHO -ne "$5" | eval "$2" > actual
  RETVAL=$?
  if cmp expected actual >/dev/null 2>/dev/null
  then
    echo "PASS: $NAME"
  else
    FAILCOUNT=$(($FAILCOUNT + 1))
    echo "FAIL: $NAME"
    [ -z "$VERBOSE" ] || diff -u expected actual
  fi
  rm -f input expected actual
  [ -z "$DEBUG" ] || set +x
  return $RETVAL
}
mkchroot()
{
  [ $
  $ECHO -n .
  dest=$1
  shift
  for i in "$@"
  do
    i=$(which $i) 
    [ -f "$dest/$i" ] && continue
    if [ -e "$i" ]
    then
      d=`echo "$i" | grep -o '.*/'` &&
      mkdir -p "$dest/$d" &&
      cat "$i" > "$dest/$i" &&
      chmod +x "$dest/$i"
    else
      echo "Not found: $i"
    fi
    mkchroot "$dest" $(ldd "$i" | egrep -o '/.* ')
  done
}
dochroot()
{
  mkdir tmpdir4chroot
  mount -t ramfs tmpdir4chroot tmpdir4chroot
  mkdir -p tmpdir4chroot/{etc,sys,proc,tmp,dev}
  cp -L testing.sh tmpdir4chroot
  $ECHO -n "Setup chroot"
  mkchroot tmpdir4chroot $*
  echo
  mknod tmpdir4chroot/dev/tty c 5 0
  mknod tmpdir4chroot/dev/null c 1 3
  mknod tmpdir4chroot/dev/zero c 1 5
  cat > tmpdir4chroot/test.sh
  chmod +x tmpdir4chroot/test.sh
  chroot tmpdir4chroot /test.sh
  umount -l tmpdir4chroot
  rmdir tmpdir4chroot
}
