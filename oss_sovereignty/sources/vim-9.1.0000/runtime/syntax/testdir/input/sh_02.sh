ccc=`echo "test"`
	ccc=`echo "test"`
case $VAR in
	x|y|z) echo xyz ;;
	a|b|c) echo abc ;;
esac
case "$aaa" in
  	bbb)  ccc=`echo $ddd|cut -b4-`
	echo "test"
	;;
	esac
echo   $VAR abc
export $VAR abc
set    $VAR abc
