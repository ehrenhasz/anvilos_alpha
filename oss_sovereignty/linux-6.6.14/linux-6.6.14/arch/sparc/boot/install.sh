if [ -f $4/vmlinuz ]; then
	mv $4/vmlinuz $4/vmlinuz.old
fi
if [ -f $4/System.map ]; then
	mv $4/System.map $4/System.old
fi
cat $2 > $4/vmlinuz
cp $3 $4/System.map
