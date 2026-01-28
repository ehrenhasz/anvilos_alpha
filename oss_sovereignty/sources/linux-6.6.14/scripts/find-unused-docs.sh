if ! [ -d "Documentation" ]; then
	echo "Run from top level of kernel tree"
	exit 1
fi
if [ "$
	echo "Usage: scripts/find-unused-docs.sh directory"
	exit 1
fi
if ! [ -d "$1" ]; then
	echo "Directory $1 doesn't exist"
	exit 1
fi
cd "$( dirname "${BASH_SOURCE[0]}" )"
cd ..
cd Documentation/
echo "The following files contain kerneldoc comments for exported functions \
that are not used in the formatted documentation"
files_included=($(grep -rHR ".. kernel-doc" --include \*.rst | cut -d " " -f 3))
declare -A FILES_INCLUDED
for each in "${files_included[@]}"; do
	FILES_INCLUDED[$each]="$each"
	done
cd ..
for file in `find $1 -name '*.c'`; do
	if [[ ${FILES_INCLUDED[$file]+_} ]]; then
	continue;
	fi
	str=$(scripts/kernel-doc -export "$file" 2>/dev/null)
	if [[ -n "$str" ]]; then
	echo "$file"
	fi
	done
