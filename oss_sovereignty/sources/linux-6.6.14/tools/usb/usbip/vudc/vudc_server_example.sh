set -e
CONFIGFS_MOUNT_POINT="/sys/kernel/config"
GADGET_NAME="g1"
ID_VENDOR="0x1d6b"
ID_PRODUCT="0x0104"
cd ${CONFIGFS_MOUNT_POINT}/usb_gadget
mkdir ${GADGET_NAME}
cd ${GADGET_NAME}
FUNC_DIR="functions/acm.ser0"
mkdir ${FUNC_DIR}
mkdir configs/c.1
ln -s ${FUNC_DIR} configs/c.1
echo ${ID_VENDOR} > idVendor
echo ${ID_PRODUCT} > idProduct
[[ -d /sys/class/udc/usbip-vudc.0 ]] || modprobe usbip-vudc num=1
echo "usbip-vudc.0" > UDC
usbipd --device &
