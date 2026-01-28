import struct, machine
USB_REQ_RECIP_INTERFACE = 0x01
USB_REQ_TYPE_CLASS = 0x20
USB_DIR_OUT = 0x00
USB_DIR_IN = 0x80
MEMORY_LAYOUT = b"@Internal Flash  /0x08000000/16*128Kg"
VID = 0x0483
PID = 0xDF11
wTransferSize = 2048
_desc_dev = bytes(
    [
        0x12,  # bLength
        0x01,  # bDescriptorType: Device
        0x00,
        0x02,  # USB version: 2.00
        0x00,  # bDeviceClass
        0x00,  # bDeviceSubClass
        0x00,  # bDeviceProtocol
        0x40,  # bMaxPacketSize
        VID & 0xFF,
        VID >> 8,  # VID
        PID & 0xFF,
        PID >> 8,  # PID
        0x00,
        0x01,  # bcdDevice: 1.00
        0x11,  # iManufacturer
        0x12,  # iProduct
        0x13,  # iSerialNumber
        0x01,  # bNumConfigurations: 1
    ]
)
_desc_cfg = bytes(
    [
        0x09,  # bLength
        0x02,  # bDescriptorType
        0x1B,
        0x00,  # wTotalLength: 27
        0x01,  # bNumInterfaces
        0x01,  # bConfigurationValue
        0x00,  # iConfiguration
        0x80,  # bmAttributes (bus powered)
        0x32,  # bMaxPower
        0x09,  # bLength
        0x04,  # bDescriptorType
        0x00,  # bInterfaceNumber
        0x00,  # bNumEndpointns
        0x00,  # bAlternateSetting
        0xFE,  # bInterfaceClass: application specific interface
        0x01,  # bInterfaceSubClasse: device firmware update
        0x02,  # bInterfaceProtocol
        0x14,  # iInterface
        0x09,  # bLength
        0x21,  # bDescriptorType
        0x0B,  # bmAttributes (will detach, upload supported, download supported)
        0xFF,
        0x00,  # wDetatchTimeout
        wTransferSize & 0xFF,
        wTransferSize >> 8,  # wTransferSize
        0x1A,
        0x01,  # bcdDFUVersion
    ]
)
_desc_strs = {
    0x11: b"iManufacturer",
    0x12: b"iProduct",
    0x13: b"iSerialNumber",
    0x14: MEMORY_LAYOUT,
}
class DFUOverUSB:
    def __init__(self, dfu):
        self.usb_buf = bytearray(wTransferSize)
        self.dfu = dfu
    def _control_xfer_cb(self, stage, request):
        bmRequestType, bRequest, wValue, wIndex, wLength = struct.unpack("<BBHHH", request)
        if stage == 1:  # SETUP
            if bmRequestType == USB_DIR_OUT | USB_REQ_TYPE_CLASS | USB_REQ_RECIP_INTERFACE:
                return memoryview(self.usb_buf)[:wLength]
            if bmRequestType == USB_DIR_IN | USB_REQ_TYPE_CLASS | USB_REQ_RECIP_INTERFACE:
                buf = memoryview(self.usb_buf)[:wLength]
                return self.dfu.handle_tx(bRequest, wValue, buf)
        elif stage == 3:  # ACK
            if bmRequestType & USB_DIR_IN:
                self.dfu.process()
            else:
                buf = memoryview(self.usb_buf)[:wLength]
                self.dfu.handle_rx(bRequest, wValue, buf)
        return True
class DFU:
    DETACH = 0
    DNLOAD = 1
    UPLOAD = 2
    GETSTATUS = 3
    CLRSTATUS = 4
    GETSTATE = 5
    ABORT = 6
    STATE_IDLE = 2
    STATE_BUSY = 4
    STATE_DNLOAD_IDLE = 5
    STATE_MANIFEST = 7
    STATE_UPLOAD_IDLE = 9
    STATE_ERROR = 0xA
    CMD_NONE = 0
    CMD_EXIT = 1
    CMD_UPLOAD = 7
    CMD_DNLOAD = 8
    CMD_DNLOAD_SET_ADDRESS = 0x21
    CMD_DNLOAD_ERASE = 0x41
    CMD_DNLOAD_READ_UNPROTECT = 0x92
    STATUS_OK = 0x00
    def __init__(self):
        self.state = DFU.STATE_IDLE
        self.cmd = DFU.CMD_NONE
        self.status = DFU.STATUS_OK
        self.error = 0
        self.leave_dfu = False
        self.addr = 0
        self.dnload_block_num = 0
        self.dnload_len = 0
        self.dnload_buf = bytearray(wTransferSize)
    def handle_rx(self, cmd, arg, buf):
        if cmd == DFU.CLRSTATUS:
            self.state = DFU.STATE_IDLE
            self.cmd = DFU.CMD_NONE
            self.status = DFU.STATUS_OK
            self.error = 0
        elif cmd == DFU.ABORT:
            self.state = DFU.STATE_IDLE
            self.cmd = DFU.CMD_NONE
            self.status = DFU.STATUS_OK
            self.error = 0
        elif cmd == DFU.DNLOAD:
            if len(buf) == 0:
                self.cmd = DFU.CMD_EXIT
            else:
                self.cmd = DFU.CMD_DNLOAD
                self.dnload_block_num = arg
                self.dnload_len = len(buf)
                self.dnload_buf[: len(buf)] = buf
    def handle_tx(self, cmd, arg, buf):
        if cmd == DFU.UPLOAD:
            if arg >= 2:
                self.cmd = DFU.CMD_UPLOAD
                addr = (arg - 2) * len(buf) + self.addr
                self.do_read(addr, buf)
                return buf
            return None
        elif cmd == DFU.GETSTATUS and len(buf) == 6:
            if self.cmd == DFU.CMD_NONE:
                pass
            elif self.cmd == DFU.CMD_EXIT:
                self.state = DFU.STATE_MANIFEST
            elif self.cmd == DFU.CMD_UPLOAD:
                self.state = DFU.STATE_UPLOAD_IDLE
            elif self.cmd == DFU.CMD_DNLOAD:
                self.state = DFU.STATE_BUSY
            else:
                self.state = DFU.STATE_BUSY
            buf[0] = self.status
            buf[1] = 0
            buf[2] = 0
            buf[3] = 0
            buf[4] = self.state
            buf[5] = self.error
            self.status = DFU.STATUS_OK
            self.error = 0
            return buf
        else:
            return None
    def process(self):
        if self.state == DFU.STATE_MANIFEST:
            self.leave_dfu = True
        elif self.state == DFU.STATE_BUSY:
            if self.cmd == DFU.CMD_DNLOAD:
                self.cmd = DFU.CMD_NONE
                self.state = self.process_dnload()
    def process_dnload(self):
        ret = -1  # Assume error.
        if self.dnload_block_num == 0:
            if self.dnload_len >= 1 and self.dnload_buf[0] == DFU.CMD_DNLOAD_ERASE:
                if self.dnload_len == 1:
                    ret = self.do_mass_erase()
                    if ret != 0:
                        self.cmd = DFU.CMD_NONE
                elif self.dnload_len == 5:
                    addr = struct.unpack_from("<L", self.dnload_buf, 1)[0]
                    ret = self.do_page_erase(addr)
            elif self.dnload_len >= 1 and self.dnload_buf[0] == DFU.CMD_DNLOAD_SET_ADDRESS:
                if self.dnload_len == 5:
                    self.addr = struct.unpack_from("<L", self.dnload_buf, 1)[0]
                    ret = 0
        elif self.dnload_block_num > 1:
            addr = (self.dnload_block_num - 2) * wTransferSize + self.addr
            ret = self.do_write(addr, self.dnload_len, self.dnload_buf)
        if ret == 0:
            return DFU.STATE_DNLOAD_IDLE
        else:
            return DFU.STATE_ERROR
    def do_mass_erase(self):
        return 0  # indicate success
    def do_page_erase(self, addr):
        return 0  # indicate success
    def do_read(self, addr, buf):
        for i in range(len(buf)):
            buf[i] = i & 0xFF
        return 0  # indicate success
    def do_write(self, addr, size, buf):
        return 0  # indicate success
dfu = DFU()
dfu_usb = DFUOverUSB(dfu)
usbd = machine.USBDevice()
usbd.active(0)
usbd.builtin_driver = usbd.BUILTIN_NONE
usbd.config(
    desc_dev=_desc_dev,
    desc_cfg=_desc_cfg,
    desc_strs=_desc_strs,
    control_xfer_cb=dfu_usb._control_xfer_cb,
)
usbd.active(1)
while not dfu.leave_dfu:
    machine.idle()
usbd.active(0)
usbd.builtin_driver = usbd.BUILTIN_DEFAULT
usbd.config(
    desc_dev=usbd.builtin_driver.desc_dev,
    desc_cfg=usbd.builtin_driver.desc_cfg,
    desc_strs=(),
)
usbd.active(1)
