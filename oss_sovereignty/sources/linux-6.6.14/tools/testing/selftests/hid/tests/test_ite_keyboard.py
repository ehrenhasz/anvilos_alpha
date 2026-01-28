from .test_keyboard import ArrayKeyboard, TestArrayKeyboard
from hidtools.util import BusType
import libevdev
import logging
logger = logging.getLogger("hidtools.test.ite-keyboard")
KERNEL_MODULE = ("itetech", "hid_ite")
class KbdData(object):
    pass
class ITEKeyboard(ArrayKeyboard):
    report_descriptor = [
        0x06, 0x85, 0xff,              
        0x09, 0x95,                    
        0xa1, 0x01,                    
        0x85, 0x5a,                    
        0x09, 0x01,                    
        0x15, 0x00,                    
        0x26, 0xff, 0x00,              
        0x75, 0x08,                    
        0x95, 0x10,                    
        0xb1, 0x00,                    
        0xc0,                          
        0x05, 0x01,                    
        0x09, 0x06,                    
        0xa1, 0x01,                    
        0x85, 0x01,                    
        0x75, 0x01,                    
        0x95, 0x08,                    
        0x05, 0x07,                    
        0x19, 0xe0,                    
        0x29, 0xe7,                    
        0x15, 0x00,                    
        0x25, 0x01,                    
        0x81, 0x02,                    
        0x95, 0x01,                    
        0x75, 0x08,                    
        0x81, 0x03,                    
        0x95, 0x05,                    
        0x75, 0x01,                    
        0x05, 0x08,                    
        0x19, 0x01,                    
        0x29, 0x05,                    
        0x91, 0x02,                    
        0x95, 0x01,                    
        0x75, 0x03,                    
        0x91, 0x03,                    
        0x95, 0x06,                    
        0x75, 0x08,                    
        0x15, 0x00,                    
        0x26, 0xff, 0x00,              
        0x05, 0x07,                    
        0x19, 0x00,                    
        0x2a, 0xff, 0x00,              
        0x81, 0x00,                    
        0xc0,                          
        0x05, 0x0c,                    
        0x09, 0x01,                    
        0xa1, 0x01,                    
        0x85, 0x02,                    
        0x19, 0x00,                    
        0x2a, 0x3c, 0x02,              
        0x15, 0x00,                    
        0x26, 0x3c, 0x02,              
        0x75, 0x10,                    
        0x95, 0x01,                    
        0x81, 0x00,                    
        0xc0,                          
        0x05, 0x01,                    
        0x09, 0x0c,                    
        0xa1, 0x01,                    
        0x85, 0x03,                    
        0x15, 0x00,                    
        0x25, 0x01,                    
        0x09, 0xc6,                    
        0x95, 0x01,                    
        0x75, 0x01,                    
        0x81, 0x06,                    
        0x75, 0x07,                    
        0x81, 0x03,                    
        0xc0,                          
        0x05, 0x88,                    
        0x09, 0x01,                    
        0xa1, 0x01,                    
        0x85, 0x04,                    
        0x19, 0x00,                    
        0x2a, 0xff, 0xff,              
        0x15, 0x00,                    
        0x26, 0xff, 0xff,              
        0x75, 0x08,                    
        0x95, 0x02,                    
        0x81, 0x02,                    
        0xc0,                          
        0x05, 0x01,                    
        0x09, 0x80,                    
        0xa1, 0x01,                    
        0x85, 0x05,                    
        0x19, 0x81,                    
        0x29, 0x83,                    
        0x15, 0x00,                    
        0x25, 0x01,                    
        0x95, 0x08,                    
        0x75, 0x01,                    
        0x81, 0x02,                    
        0xc0,                          
    ]
    def __init__(
        self,
        rdesc=report_descriptor,
        name=None,
        input_info=(BusType.USB, 0x06CB, 0x2968),
    ):
        super().__init__(rdesc, name, input_info)
    def event(self, keys, reportID=None, application=None):
        application = application or "Keyboard"
        return super().event(keys, reportID, application)
class TestITEKeyboard(TestArrayKeyboard):
    kernel_modules = [KERNEL_MODULE]
    def create_device(self):
        return ITEKeyboard()
    def test_wifi_key(self):
        uhdev = self.uhdev
        syn_event = self.syn_event
        r = [0x03, 0x00]
        uhdev.call_input_event(r)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_RFKILL, 1))
        events = uhdev.next_sync_events()
        self.debug_reports([r], uhdev, events)
        self.assertInputEventsIn(expected, events)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_RFKILL, 0))
        self.debug_reports([], uhdev, events)
        self.assertInputEventsIn(expected, events)
