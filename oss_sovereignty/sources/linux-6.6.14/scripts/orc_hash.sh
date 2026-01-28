set -e
printf '%s' '
awk '
/^
/^struct orc_entry {$/ { p=1 }
p { print }
/^}/ { p=0 }' |
	sha1sum |
	cut -d " " -f 1 |
	sed 's/\([0-9a-f]\{2\}\)/0x\1,/g'
