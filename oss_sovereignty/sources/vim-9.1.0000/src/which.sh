IFS=":"
for ac_dir in $PATH; do
	if test -f "$ac_dir/$1"; then
		echo "$ac_dir/$1"
		break
	fi
done
