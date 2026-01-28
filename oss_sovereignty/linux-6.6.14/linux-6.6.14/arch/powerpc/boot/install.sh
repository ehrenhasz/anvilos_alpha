set -e
image_name=`basename $2`
if [ -f $4/$image_name ]; then
	mv $4/$image_name $4/$image_name.old
fi
if [ -f $4/System.map ]; then
	mv $4/System.map $4/System.old
fi
cat $2 > $4/$image_name
cp $3 $4/System.map
