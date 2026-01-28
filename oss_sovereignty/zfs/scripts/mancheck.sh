trap 'rm -f "$stdout_file" "$stderr_file" "$result_file"' EXIT
if [ "$#" -eq 0 ]; then
    echo "Usage: $0 manpage-directory..."
    exit 1
fi
if ! command -v mandoc > /dev/null; then
    echo "skipping mancheck because mandoc is not installed"
    exit 0
fi
IFS="
"
files="$(find "$@" -type f -name '*[1-9]*' -not -name '.*')" || exit 1
add_excl="$(awk '
    /^.\\" lint-ok:/ {
        print "-e"
        $1 = "mandoc:"
        $2 = FILENAME ":[[:digit:]]+:[[:digit:]]+:"
        print
    }' $files)"
stdout_file="$(mktemp)"
stderr_file="$(mktemp)"
mandoc -Tlint $files 1>"$stdout_file" 2>"$stderr_file"
result_file="$(mktemp)"
grep -vhE -e 'mandoc: outdated mandoc.db' -e 'STYLE: referenced manual not found' $add_excl "$stdout_file" "$stderr_file" > "$result_file"
if [ -s "$result_file" ]; then
    cat "$result_file"
    exit 1
else
    echo "no errors found"
fi
