from .test_keyboard import ArrayKeyboard, TestArrayKeyboard
from hidtools.util import BusType
import libevdev
import logging
logger = logging.getLogger("hidtools.test.apple-keyboard")
KERNEL_MODULE = ("apple", "hid-apple")
class KbdData(object):
    pass
class AppleKeyboard(ArrayKeyboard):
    report_descriptor = [
        0x05, 0x01,         
        0x09, 0x06,         
        0xa1, 0x01,         
        0x85, 0x01,         
        0x05, 0x07,         
        0x19, 0xe0,         
        0x29, 0xe7,         
        0x15, 0x00,         
        0x25, 0x01,         
        0x75, 0x01,         
        0x95, 0x08,         
        0x81, 0x02,         
        0x75, 0x08,         
        0x95, 0x01,         
        0x81, 0x01,         
        0x75, 0x01,         
        0x95, 0x05,         
        0x05, 0x08,         
        0x19, 0x01,         
        0x29, 0x05,         
        0x91, 0x02,         
        0x75, 0x03,         
        0x95, 0x01,         
        0x91, 0x01,         
        0x75, 0x08,         
        0x95, 0x06,         
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
        0x85, 0x47,         
        0x05, 0x01,         
        0x09, 0x06,         
        0xa1, 0x02,         
        0x05, 0x06,         
        0x09, 0x20,         
        0x15, 0x00,         
        0x26, 0xff, 0x00,   
        0x75, 0x08,         
        0x95, 0x01,         
        0x81, 0x02,         
        0xc0,               
        0xc0,               
        0x05, 0x0c,         
        0x09, 0x01,         
        0xa1, 0x01,         
        0x85, 0x11,         
        0x15, 0x00,         
        0x25, 0x01,         
        0x75, 0x01,         
        0x95, 0x03,         
        0x81, 0x01,         
        0x75, 0x01,         
        0x95, 0x01,         
        0x05, 0x0c,         
        0x09, 0xb8,         
        0x81, 0x02,         
        0x06, 0xff, 0x00,   
        0x09, 0x03,         
        0x81, 0x02,         
        0x75, 0x01,         
        0x95, 0x03,         
        0x81, 0x01,         
        0x05, 0x0c,         
        0x85, 0x12,         
        0x15, 0x00,         
        0x25, 0x01,         
        0x75, 0x01,         
        0x95, 0x01,         
        0x09, 0xcd,         
        0x81, 0x02,         
        0x09, 0xb3,         
        0x81, 0x02,         
        0x09, 0xb4,         
        0x81, 0x02,         
        0x09, 0xb5,         
        0x81, 0x02,         
        0x09, 0xb6,         
        0x81, 0x02,         
        0x81, 0x01,         
        0x81, 0x01,         
        0x81, 0x01,         
        0x85, 0x13,         
        0x15, 0x00,         
        0x25, 0x01,         
        0x75, 0x01,         
        0x95, 0x01,         
        0x06, 0x01, 0xff,   
        0x09, 0x0a,         
        0x81, 0x02,         
        0x06, 0x01, 0xff,   
        0x09, 0x0c,         
        0x81, 0x22,         
        0x75, 0x01,         
        0x95, 0x06,         
        0x81, 0x01,         
        0x85, 0x09,         
        0x09, 0x0b,         
        0x75, 0x08,         
        0x95, 0x01,         
        0xb1, 0x02,         
        0x75, 0x08,         
        0x95, 0x02,         
        0xb1, 0x01,         
        0xc0,               
    ]
    def __init__(
        self,
        rdesc=report_descriptor,
        name="Apple Wireless Keyboard",
        input_info=(BusType.BLUETOOTH, 0x05AC, 0x0256),
    ):
        super().__init__(rdesc, name, input_info)
        self.default_reportID = 1
    def send_fn_state(self, state):
        data = KbdData()
        setattr(data, "0xff0003", state)
        r = self.create_report(data, reportID=17)
        self.call_input_event(r)
        return [r]
class TestAppleKeyboard(TestArrayKeyboard):
    kernel_modules = [KERNEL_MODULE]
    def create_device(self):
        return AppleKeyboard()
    def test_single_function_key(self):
        """check for function key reliability."""
        uhdev = self.uhdev
        evdev = uhdev.get_evdev()
        syn_event = self.syn_event
        r = uhdev.event(["F4"])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_ALL_APPLICATIONS, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_ALL_APPLICATIONS] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 0
        r = uhdev.event([])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_ALL_APPLICATIONS, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_ALL_APPLICATIONS] == 0
    def test_single_fn_function_key(self):
        """check for function key reliability with the fn key."""
        uhdev = self.uhdev
        evdev = uhdev.get_evdev()
        syn_event = self.syn_event
        r = uhdev.send_fn_state(1)
        r.extend(uhdev.event(["F4"]))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F4, 1))
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 1
        r = uhdev.event([])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F4, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 1
        r = uhdev.send_fn_state(0)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
    def test_single_fn_function_key_release_first(self):
        """check for function key reliability with the fn key."""
        uhdev = self.uhdev
        evdev = uhdev.get_evdev()
        syn_event = self.syn_event
        r = uhdev.send_fn_state(1)
        r.extend(uhdev.event(["F4"]))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F4, 1))
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 1
        r = uhdev.send_fn_state(0)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        r = uhdev.event([])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F4, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
    def test_single_fn_function_key_inverted(self):
        """check for function key reliability with the fn key."""
        uhdev = self.uhdev
        evdev = uhdev.get_evdev()
        syn_event = self.syn_event
        r = uhdev.event(["F4"])
        r.extend(uhdev.send_fn_state(1))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_ALL_APPLICATIONS, 1))
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_ALL_APPLICATIONS] == 1
        r = uhdev.event([])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_ALL_APPLICATIONS, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_ALL_APPLICATIONS] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 1
        r = uhdev.send_fn_state(0)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
    def test_multiple_fn_function_key_release_first(self):
        """check for function key reliability with the fn key."""
        uhdev = self.uhdev
        evdev = uhdev.get_evdev()
        syn_event = self.syn_event
        r = uhdev.send_fn_state(1)
        r.extend(uhdev.event(["F4"]))
        r.extend(uhdev.event(["F4", "F6"]))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F4, 1))
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F6, 1))
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_F6] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 1
        r = uhdev.event(["F6"])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F4, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_F6] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 1
        r = uhdev.send_fn_state(0)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_F6] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 0
        r = uhdev.event([])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F6, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_F6] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 0
    def test_multiple_fn_function_key_release_between(self):
        """check for function key reliability with the fn key."""
        uhdev = self.uhdev
        evdev = uhdev.get_evdev()
        syn_event = self.syn_event
        r = uhdev.event(["F4"])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_ALL_APPLICATIONS, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_ALL_APPLICATIONS] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_F6] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_KBDILLUMUP] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 0
        r = uhdev.send_fn_state(1)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_ALL_APPLICATIONS] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_F6] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_KBDILLUMUP] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 1
        r = uhdev.event(["F4", "F6"])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F6, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_ALL_APPLICATIONS] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_F6] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_KBDILLUMUP] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 1
        r = uhdev.event(["F4", "F6"])
        expected = []
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_ALL_APPLICATIONS] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_F6] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_KBDILLUMUP] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 1
        r = uhdev.send_fn_state(0)
        r.extend(uhdev.event([]))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_ALL_APPLICATIONS, 0))
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_F6, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_F4] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_ALL_APPLICATIONS] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_F6] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_KBDILLUMUP] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 0
    def test_single_pageup_key_release_first(self):
        """check for function key reliability with the [page] up key."""
        uhdev = self.uhdev
        evdev = uhdev.get_evdev()
        syn_event = self.syn_event
        r = uhdev.send_fn_state(1)
        r.extend(uhdev.event(["UpArrow"]))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_PAGEUP, 1))
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_PAGEUP] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_UP] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 1
        r = uhdev.send_fn_state(0)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_FN, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_PAGEUP] == 1
        assert evdev.value[libevdev.EV_KEY.KEY_UP] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 0
        r = uhdev.event([])
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_KEY.KEY_PAGEUP, 0))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEventsIn(expected, events)
        assert evdev.value[libevdev.EV_KEY.KEY_PAGEUP] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_UP] == 0
        assert evdev.value[libevdev.EV_KEY.KEY_FN] == 0
