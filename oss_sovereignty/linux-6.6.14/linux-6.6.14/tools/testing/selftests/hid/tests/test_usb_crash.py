from . import base
import pytest
import logging
logger = logging.getLogger("hidtools.test.usb")
class USBDev(base.UHIDTestDevice):
    report_descriptor = [
        0x05, 0x01,  
        0x09, 0x02,  
        0xa1, 0x01,  
        0x09, 0x02,  
        0xa1, 0x02,  
        0x09, 0x01,  
        0xa1, 0x00,  
        0x05, 0x09,  
        0x19, 0x01,  
        0x29, 0x03,  
        0x15, 0x00,  
        0x25, 0x01,  
        0x75, 0x01,  
        0x95, 0x03,  
        0x81, 0x02,  
        0x75, 0x05,  
        0x95, 0x01,  
        0x81, 0x03,  
        0x05, 0x01,  
        0x09, 0x30,  
        0x09, 0x31,  
        0x15, 0x81,  
        0x25, 0x7f,  
        0x75, 0x08,  
        0x95, 0x02,  
        0x81, 0x06,  
        0xc0,        
        0xc0,        
        0xc0,        
    ]
    def __init__(self, name=None, input_info=None):
        super().__init__(
            name, "Mouse", input_info=input_info, rdesc=USBDev.report_descriptor
        )
    def is_ready(self):
        return True
    def get_evdev(self, application=None):
        return "OK"
class TestUSBDevice(base.BaseTestCase.TestUhid):
    """
    Test class to test if an emulated USB device crashes
    the kernel.
    """
    @pytest.fixture()
    def new_uhdev(self, usbVidPid, request):
        self.module, self.vid, self.pid = usbVidPid
        self._load_kernel_module(None, self.module)
        return USBDev(input_info=(3, self.vid, self.pid))
    def test_creation(self):
        """
        inject the USB dev through uhid and immediately see if there is a crash:
        uhid can create a USB device with the BUS_USB bus, and some
        drivers assume that they can then access USB related structures
        when they are actually provided a uhid device. This leads to
        a crash because those access result in a segmentation fault.
        The kernel should not crash on any (random) user space correct
        use of its API. So run through all available modules and declared
        devices to see if we can generate a uhid device without a crash.
        The test is empty as the fixture `check_taint` is doing the job (and
        honestly, when the kernel crashes, the whole machine freezes).
        """
        assert True
