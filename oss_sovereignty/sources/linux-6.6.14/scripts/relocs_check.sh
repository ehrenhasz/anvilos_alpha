objdump="$1"
nm="$2"
vmlinux="$3"
undef_weak_symbols=$($nm "$vmlinux" | awk '$1 ~ /w/ { print $2 }')
$objdump -R "$vmlinux" |
	grep -E '\<R_' |
	([ "$undef_weak_symbols" ] && grep -F -w -v "$undef_weak_symbols" || cat)
