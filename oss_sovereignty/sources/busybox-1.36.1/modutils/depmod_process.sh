while read -r word rest; do
    if ! test "${word/*:/}"; then
	echo -n "$word "
	echo "$rest" | xargs -n1 | sort | xargs
    else
	echo "$word $rest";
    fi
done
