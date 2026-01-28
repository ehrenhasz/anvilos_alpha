if [ ! -x "$(command -v "$@")" ]; then
	echo 0
	exit 1
fi
"$@" --version | sed -E 's/v([0-9]+)\.([0-9]+)/\1\2/'
