[ "${0%/*}" = "$0" ] || cd "${0%/*}" || exit
automake --version | awk '{print $NF; exit}' | (
	IFS=. read -r AM_MAJ AM_MIN _
	[ "$AM_MAJ" -gt 1 ] || [ "$AM_MIN" -ge 14 ]
) || {
	process_root() {
		root="$1"; shift
		grep -q '%[CD]%' "$root/Makefile.am" || return
		find "$root" -name Makefile.am "$@" | while read -r dir; do
			dir="${dir%/Makefile.am}"
			grep -q '%[CD]%' "$dir/Makefile.am" || continue
			reldir="${dir
			reldir="${reldir
			canon_reldir="$(printf '%s' "$reldir" | tr -C 'a-zA-Z0-9@_' '_')"
			reldir_slash="$reldir/"
			canon_reldir_slash="${canon_reldir}_"
			[ -z "$reldir" ] && reldir_slash=
			[ -z "$reldir" ] && canon_reldir_slash=
			echo "$dir/Makefile.am" >&3
			sed -i~ -e "s:%D%/:$reldir_slash:g"       -e "s:%D%:$reldir:g" \
			        -e "s:%C%_:$canon_reldir_slash:g" -e "s:%C%:$canon_reldir:g" "$dir/Makefile.am"
		done 3>>"$substituted_files"
	}
	rollback() {
		while read -r f; do
			mv "$f~" "$f"
		done < "$substituted_files"
		rm -f "$substituted_files"
	}
	echo "Automake <1.14; papering over missing %reldir%/%canon_reldir% support" >&2
	substituted_files="$(mktemp)"
	trap rollback EXIT
	roots="$(sed '/Makefile$/!d;/module/d;s:^\s*:./:;s:/Makefile::;/^\.$/d' configure.ac)"
	IFS="
"
	for root in $roots; do
		root="${root
		process_root "$root"
	done
	set -f
	process_root . $(printf '!\n-path\n%s/*\n' $roots)
}
autoreconf -fiv && rm -rf autom4te.cache
