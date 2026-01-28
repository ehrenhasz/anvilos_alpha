import machine
VID = 0xF055
PID = 0x9999
EP_OUT = 0x01
EP_IN = 0x81
_desc_dev = bytes(
    [
        0x12,  
        0x01,  
        0x00,
        0x02,  
        0xFF,  
        0x00,  
        0x01,  
        0x40,  
        VID & 0xFF,
        VID >> 8 & 0xFF,  
        PID & 0xFF,
        PID >> 8 & 0xFF,  
        0x00,
        0x01,  
        0x11,  
        0x12,  
        0x13,  
        0x01,  
    ]
)
_desc_cfg = bytes(
    [
        0x09,  
        0x02,  
        0x20,
        0x00,  
        0x01,  
        0x01,  
        0x14,  
        0xA0,  
        0x96,  
        0x09,  
        0x04,  
        0x00,  
        0x00,  
        0x02,  
        0xFF,  
        0x03,  
        0x00,  
        0x15,  
        0x07,  
        0x05,  
        EP_IN,  
        0x03,  
        0x40,
        0x00,  
        0x0A,  
        0x07,  
        0x05,  
        EP_OUT,  
        0x02,  
        0x40,
        0x00,  
        0x00,  
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
