temp="/tmp/fix_ws.$$.$RANDOM"
begin8sp_tab=$'s/^        /\t/'
beginchar7sp_chartab=$'s/^\\([^ \t]\\)       /\\1\t/'
tab8sp_tabtab=$'s/\t        /\t\t/g'
tab8sptab_tabtabtab=$'s/\t        \t/\t\t\t/g'
begin17sptab_tab=$'s/^ \\{1,7\\}\t/\t/'
tab17sptab_tabtab=$'s/\t \\{1,7\\}\t/\t\t/g'
trailingws_=$'s/[ \t]*$//'
find "$@" -type f \
| while read name; do
    test "YES" = "${name/*.bz2/YES}" && continue
    test "YES" = "${name/*.gz/YES}" && continue
    test "YES" = "${name/*.png/YES}" && continue
    test "YES" = "${name/*.gif/YES}" && continue
    test "YES" = "${name/*.jpg/YES}" && continue
    test "YES" = "${name/*.diff/YES}" && continue
    test "YES" = "${name/*.patch/YES}" && continue
    test "YES" = "${name/*.right/YES}" && continue
    if test "YES" = "${name/*.[chsS]/YES}" \
	-o "YES" = "${name/*.sh/YES}" \
	-o "YES" = "${name/*.txt/YES}" \
	-o "YES" = "${name/*.html/YES}" \
	-o "YES" = "${name/*.htm/YES}" \
	-o "YES" = "${name/*Config.in*/YES}" \
    ; then
	echo "Formatting: $name" >&2
	cat "$name" \
	| sed -e "$tab8sptab_tabtabtab" -e "$tab8sptab_tabtabtab" \
	      -e "$tab8sptab_tabtabtab" -e "$tab8sptab_tabtabtab" \
	| sed "$begin17sptab_tab" \
	| sed -e "$tab17sptab_tabtab" -e "$tab17sptab_tabtab" \
	      -e "$tab17sptab_tabtab" -e "$tab17sptab_tabtab" \
	      -e "$tab17sptab_tabtab" -e "$tab17sptab_tabtab" \
	| sed "$trailingws_"
    elif test "YES" = "${name/*Makefile*/YES}" \
	-o "YES" = "${name/*Kbuild*/YES}" \
    ; then
	echo "Makefile: $name" >&2
	cat "$name" \
	| sed -e "$tab8sptab_tabtabtab" -e "$tab8sptab_tabtabtab" \
	      -e "$tab8sptab_tabtabtab" -e "$tab8sptab_tabtabtab" \
	| sed -e "$tab17sptab_tabtab" -e "$tab17sptab_tabtab" \
	      -e "$tab17sptab_tabtab" -e "$tab17sptab_tabtab" \
	      -e "$tab17sptab_tabtab" -e "$tab17sptab_tabtab" \
	| sed "$trailingws_"
    else
	echo "Removing trailing whitespace: $name" >&2
	cat "$name" \
	| sed "$trailingws_"
    fi >"$temp"
    cat "$temp" >"$name"
done
rm "$temp" 2>/dev/null
