main()
{
	if [ "$#" -eq 0 ]; then
		echo "$0: Generating default compression gzip"
		copts="cvzf"
		ext=".tar.gz"
	else
		case "$1" in
			tar)
				copts="cvf"
				ext=".tar"
				;;
			targz)
				copts="cvzf"
				ext=".tar.gz"
				;;
			tarbz2)
				copts="cvjf"
				ext=".tar.bz2"
				;;
			tarxz)
				copts="cvJf"
				ext=".tar.xz"
				;;
			*)
			echo "Unknown tarball format $1"
			exit 1
			;;
	esac
	fi
	dest=`pwd`
	install_work="$dest"/kselftest_install
	install_name=kselftest
	install_dir="$install_work"/"$install_name"
	mkdir -p "$install_dir"
	./kselftest_install.sh "$install_dir"
	(cd "$install_work"; tar $copts "$dest"/kselftest${ext} $install_name)
	echo -e "\nConsider using 'make gen_tar' instead of this script\n"
	echo "Kselftest archive kselftest${ext} created!"
	rm -rf "$install_work"
}
main "$@"
