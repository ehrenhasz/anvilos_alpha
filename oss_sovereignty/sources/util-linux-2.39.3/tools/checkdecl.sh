if [ ! -f ./configure ]; then
	echo "Not found configure script"
	exit 1
fi
for decl in $(awk '/HAVE_DECL_.*ac_have_decl/ { print $2 }' configure); do
	git grep -nE '[[:blank:]]*#[[:blank:]]*if(ndef|def)[[:blank:]]*'$decl;
done | sort -u
