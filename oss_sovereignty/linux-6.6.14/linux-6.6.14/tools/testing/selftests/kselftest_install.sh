main()
{
	base_dir=`pwd`
	install_dir="$base_dir"/kselftest_install
	if [ $(basename "$base_dir") !=  "selftests" ]; then
		echo "$0: Please run it in selftests directory ..."
		exit 1;
	fi
	if [ "$#" -eq 0 ]; then
		echo "$0: Installing in default location - $install_dir ..."
	elif [ ! -d "$1" ]; then
		echo "$0: $1 doesn't exist!!"
		exit 1;
	else
		install_dir="$1"
		echo "$0: Installing in specified location - $install_dir ..."
	fi
	KSFT_INSTALL_PATH="$install_dir" make install
}
main "$@"
