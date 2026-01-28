if [ "$(basename $2)" = "Image.gz" ]; then
  echo "Installing compressed kernel"
  base=vmlinuz
else
  echo "Installing normal kernel"
  base=vmlinux
fi
if [ -f $4/$base-$1 ]; then
  mv $4/$base-$1 $4/$base-$1.old
fi
cat $2 > $4/$base-$1
if [ -f $4/System.map-$1 ]; then
  mv $4/System.map-$1 $4/System.map-$1.old
fi
cp $3 $4/System.map-$1
