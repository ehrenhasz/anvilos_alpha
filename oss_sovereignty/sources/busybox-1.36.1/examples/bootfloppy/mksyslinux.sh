dummy=""
if [ -z `which mformat` -o -z `which mcopy` ]; then
	echo "You must have the mtools package installed to run this script"
	exit 1
fi
if [ -z "$1" ]; then
	echo "usage: `basename $0` path/to/linux/kernel"
	exit 1
fi
if [ ! -f rootfs.gz ]; then
	echo "You need to have a rootfs built first."
	echo "Hit RETURN to make one now or Control-C to quit."
	read dummy
	./mkrootfs.sh
fi
echo "Please insert a blank floppy in the drive and press RETURN to format"
echo "(WARNING: All data will be erased! Hit Control-C to abort)"
read dummy
echo "Formatting the floppy..."
mformat a:
echo "Making it bootable with Syslinux..."
syslinux -s /dev/fd0
echo "Copying Syslinux configuration files..."
mcopy syslinux.cfg display.txt a:
echo "Copying root filesystem file..."
mcopy rootfs.gz a:
echo "Copying linux kernel..."
mcopy $1 a:linux
echo "Finished: boot floppy created"
