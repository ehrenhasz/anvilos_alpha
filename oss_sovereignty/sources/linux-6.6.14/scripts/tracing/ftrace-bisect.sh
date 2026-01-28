if [ $
  echo 'usage: ftrace-bisect full-file test-file  non-test-file'
  exit
fi
full=$1
test=$2
nontest=$3
x=`cat $full | wc -l`
if [ $x -eq 1 ]; then
	echo "There's only one function left, must be the bad one"
	cat $full
	exit 0
fi
let x=$x/2
let y=$x+1
if [ ! -f $full ]; then
	echo "$full does not exist"
	exit 1
fi
if [ -f $test ]; then
	echo -n "$test exists, delete it? [y/N]"
	read a
	if [ "$a" != "y" -a "$a" != "Y" ]; then
		exit 1
	fi
fi
if [ -f $nontest ]; then
	echo -n "$nontest exists, delete it? [y/N]"
	read a
	if [ "$a" != "y" -a "$a" != "Y" ]; then
		exit 1
	fi
fi
sed -ne "1,${x}p" $full > $test
sed -ne "$y,\$p" $full > $nontest
