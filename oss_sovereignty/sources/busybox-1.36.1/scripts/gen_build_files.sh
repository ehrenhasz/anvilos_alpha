export LC_ALL=C
test $
cd -- "$2" || { echo "Syntax: $0 SRCTREE OBJTREE"; exit 1; }
mkdir include 2>/dev/null
srctree="$1"
status() { printf '  %-8s%s\n' "$1" "$2"; }
gen() { status "GEN" "$@"; }
chk() { status "CHK" "$@"; }
custom_scripts()
{
	custom_loc="$1"
	if [ -d "$custom_loc" ]
	then
		for i in $(cd "$custom_loc"; ls * 2>/dev/null)
		do
			printf "IF_FEATURE_SH_EMBEDDED_SCRIPTS(APPLET_SCRIPTED(%s, scripted, BB_DIR_USR_BIN, BB_SUID_DROP, scripted))\n" $i;
		done
	fi
}
generate()
{
	src="$1"
	dst="$2"
	header="$3"
	loc="$4"
	{
		printf "%s\n" "${header}"
		sed -n '/^INSERT$/ q; p' "${src}"
		cat
		if [ -n "$loc" ]
		then
			custom_scripts "$loc"
		fi
		sed -n '/^INSERT$/ {
		:l
		    n
		    p
		    bl
		}' "${src}"
	} >"${dst}.tmp"
	if ! cmp -s "${dst}" "${dst}.tmp"; then
		gen "${dst}"
		mv "${dst}.tmp" "${dst}"
	else
		rm -f "${dst}.tmp"
	fi
}
sed -n 's@^//applet:@@p' "$srctree"/*/*.c "$srctree"/*/*/*.c \
| generate \
	"$srctree/include/applets.src.h" \
	"include/applets.h" \
	"/* DO NOT EDIT. This file is generated from applets.src.h */" \
	"$srctree/embed"
TAB="$(printf '\tX')"
TAB="${TAB%X}"
LF="$(printf '\nX')"
LF="${LF%X}"
sed -n -e 's@^//usage:\([ '"$TAB"'].*\)$@\1 \\@p' \
       -e 's@^//usage:\([^ '"$TAB"'].*\)$@\'"$LF"'\1 \\@p' \
	"$srctree"/*/*.c "$srctree"/*/*/*.c \
| generate \
	"$srctree/include/usage.src.h" \
	"include/usage.h" \
	"/* DO NOT EDIT. This file is generated from usage.src.h */"
{ cd -- "$srctree" && find . -type d ! '(' -name '.?*' -prune ')'; } \
| while read -r d; do
	d="${d
	src="$srctree/$d/Kbuild.src"
	dst="$d/Kbuild"
	if test -f "$src"; then
		mkdir -p -- "$d" 2>/dev/null
		sed -n 's@^//kbuild:@@p' "$srctree/$d"/*.c \
		| generate \
			"${src}" "${dst}" \
			"
	fi
	src="$srctree/$d/Config.src"
	dst="$d/Config.in"
	if test -f "$src"; then
		mkdir -p -- "$d" 2>/dev/null
		sed -n 's@^//config:@@p' "$srctree/$d"/*.c \
		| generate \
			"${src}" "${dst}" \
			"
	fi
done
exit 0
