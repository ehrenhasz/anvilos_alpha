if [ "$
    echo "Usage: appsign.sh *build dir*"
    exit 1
fi
BUILD=$1
if [ `uname -s` = "Darwin" ]; then
echo -n `md5 -q $BUILD/application.bin` > __md5hash.bin
else
echo -n `md5sum --binary $BUILD/application.bin | awk '{ print $1 }'` > __md5hash.bin
fi
cat $BUILD/application.bin __md5hash.bin > $BUILD/mcuimg.bin
RET=$?
rm -f __md5hash.bin
rm -f $BUILD/application.bin
exit $RET
