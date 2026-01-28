if [ "$#" -ne 1 ]; then
    echo "Usage: bootgen.sh *build dir*"
    exit 1
fi
BUILD=$1
RELOCATOR=bootmgr/relocator
BOOTMGR=${BUILD}
if [ ! -f $RELOCATOR/relocator.bin ]; then
	echo "Error : Relocator Not found!"
	exit 1
else
	echo "Relocator found..."
fi
if [ ! -f $BOOTMGR/bootmgr.bin ]; then
	echo "Error : Boot Manager Not found!"
	exit 1
else
	echo "Boot Manager found..."
fi
echo "Generating bootloader..."
dd if=/dev/zero of=__tmp.bin ibs=1 count=256 conv=notrunc >/dev/null 2>&1
dd if=$RELOCATOR/relocator.bin of=__tmp.bin ibs=1 conv=notrunc >/dev/null 2>&1
cat __tmp.bin $BOOTMGR/bootmgr.bin > $BOOTMGR/bootloader.bin
rm -f __tmp.bin
rm -f $BOOTMGR/bootmgr.bin
