makeopts="-j9"
test -f include/applets.h || { echo "No include/applets.h file"; exit 1; }
apps="`
grep ^IF_ include/applets.h \
| grep -v '^IF_FEATURE_' \
| sed 's/IF_\([A-Z0-9._-]*\)(.*/\1/' \
| grep -v '^BUSYBOX$' \
| sort | uniq
`"
test -f .config || { echo "No .config file"; exit 1; }
cfg="`cat .config`"
allno="$cfg"
for app in $apps; do
	allno="`echo "$allno" | sed "s/^CONFIG_${app}=y\$/# CONFIG_${app} is not set/"`"
done
allno="`echo "$allno" | sed "s/^CONFIG_BUSYBOX=y\$/# CONFIG_BUSYBOX is not set/"`"
allno="`echo "$allno" | sed "s/^\(CONFIG_.*_DEPENDENCIES\)=y\$/# \1 is not set/"`"
trap 'test -f .config.SV && mv .config.SV .config && touch .config' EXIT
test $# = 0 && set -- $apps
fail=0
for app; do
	{ echo "$cfg" | grep -q "^CONFIG_${app}=y\$"; } || continue
	echo "Making ${app}..."
	mv .config .config.SV
	echo "CONFIG_${app}=y" >.config
	echo "$allno" | sed "/^# CONFIG_${app} is not set\$/d" >>.config
	if test x"${app}" != x"SH_IS_ASH" && test x"${app}" != x"SH_IS_HUSH"; then
		sed '/CONFIG_SH_IS_NONE/d' -i .config
		echo "CONFIG_SH_IS_NONE=y" >>.config
	fi
	if ! yes '' | make oldconfig >busybox_make_${app}.log 2>&1; then
		fail=$((fail+1))
		echo "Config error for ${app}"
		mv .config busybox_config_${app}
	elif ! make $makeopts >>busybox_make_${app}.log 2>&1; then
		fail=$((fail+1))
		grep -i -e error: -e warning: busybox_make_${app}.log
		echo "Build error for ${app}"
		mv .config busybox_config_${app}
	elif ! grep -q '^#define NUM_APPLETS 1$' include/NUM_APPLETS.h; then
		grep -i -e error: -e warning: busybox_make_${app}.log
		mv busybox busybox_${app}
		fail=$((fail+1))
		echo "NUM_APPLETS != 1 for ${app}: `cat include/NUM_APPLETS.h`"
		mv .config busybox_config_${app}
	else
		if grep -q 'use larger COMMON_BUFSIZE' busybox_make_${app}.log; then
			tail -n1 busybox_make_${app}.log
			nm busybox_unstripped | grep ' _end'
			make >/dev/null 2>&1
			nm busybox_unstripped | grep ' _end'
			grep ^bb_common_bufsiz1 busybox_unstripped.map
		fi
		grep -i -e error: -e warning: busybox_make_${app}.log \
		|| rm busybox_make_${app}.log
		mv busybox busybox_${app}
	fi
	mv .config.SV .config
done
touch .config # or else next "make" can be confused
echo "Failures: $fail"
test $fail = 0 # set exitcode
