import machine
VID = 0xF055
PID = 0x9999
EP_OUT = 0x01
EP_IN = 0x81
_desc_dev = bytes(
    [
        0x12,  # bLength
        0x01,  # bDescriptorType: Device
        0x00,
        0x02,  # USB version: 2.00
        0xFF,  # bDeviceClass: vendor
        0x00,  # bDeviceSubClass
        0x01,  # bDeviceProtocol
        0x40,  # bMaxPacketSize
        VID & 0xFF,
        VID >> 8 & 0xFF,  # VID
        PID & 0xFF,
        PID >> 8 & 0xFF,  # PID
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
        0x02,  # bDescriptorType: configuration
        0x20,
        0x00,  # wTotalLength: 32
        0x01,  # bNumInterfaces
        0x01,  # bConfigurationValue
        0x14,  # iConfiguration
        0xA0,  # bmAttributes
        0x96,  # bMaxPower
        0x09,  # bLength
        0x04,  # bDescriptorType: interface
        0x00,  # bInterfaceNumber
        0x00,  # bAlternateSetting
        0x02,  # bNumEndpoints
        0xFF,  # bInterfaceClass
        0x03,  # bInterfaceSubClass
        0x00,  # bInterfaceProtocol
        0x15,  # iInterface
        0x07,  # bLength
        0x05,  # bDescriptorType: endpoint
        EP_IN,  # bEndpointAddress
        0x03,  # bmAttributes: interrupt
        0x40,
        0x00,  # wMaxPacketSize
        0x0A,  # bInterval
        0x07,  # bLength
        0x05,  # bDescriptorType: endpoint
        EP_OUT,  # bEndpointAddress
        0x02,  # bmAttributes: bulk
        0x40,
        0x00,  # wMaxPacketSize
        0x00,  # bInterval
    ]
)
_desc_strs = {
    0x11: b"iManufacturer",
    0x12: b"iProduct",
    0x13: b"iSerial",
    0x14: b"iInterface",
    0x15: b"iInterface",
    0x16: b"Extra information",
}
def _open_itf_cb(interface_desc_view):
    print("_open_itf_cb", bytes(interface_desc_view))
    usbd.submit_xfer(EP_OUT, usbd_buf)
def _xfer_cb(ep_addr, result, xferred_bytes):
    print("_xfer_cb", ep_addr, result, xferred_bytes)
    if ep_addr == EP_OUT:
        print(usbd_buf)
        usbd.submit_xfer(EP_IN, memoryview(usbd_buf)[:xferred_bytes])
    elif ep_addr == EP_IN:
        usbd.submit_xfer(EP_OUT, usbd_buf)
usbd_buf = bytearray(64)
usbd = machine.USBDevice()
usbd.builtin_driver = usbd.BUILTIN_NONE
usbd.config(
    desc_dev=_desc_dev,
    desc_cfg=_desc_cfg,
    desc_strs=_desc_strs,
    open_itf_cb=_open_itf_cb,
    xfer_cb=_xfer_cb,
)
usbd.active(1)
