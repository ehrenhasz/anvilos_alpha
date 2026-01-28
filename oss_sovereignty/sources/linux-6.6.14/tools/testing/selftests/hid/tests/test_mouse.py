from . import base
import hidtools.hid
from hidtools.util import BusType
import libevdev
import logging
import pytest
logger = logging.getLogger("hidtools.test.mouse")
try:
    libevdev.EV_REL.REL_WHEEL_HI_RES
except AttributeError:
    libevdev.EV_REL.REL_WHEEL_HI_RES = libevdev.EV_REL.REL_0B
    libevdev.EV_REL.REL_HWHEEL_HI_RES = libevdev.EV_REL.REL_0C
class InvalidHIDCommunication(Exception):
    pass
class MouseData(object):
    pass
class BaseMouse(base.UHIDTestDevice):
    def __init__(self, rdesc, name=None, input_info=None):
        assert rdesc is not None
        super().__init__(name, "Mouse", input_info=input_info, rdesc=rdesc)
        self.left = False
        self.right = False
        self.middle = False
    def create_report(self, x, y, buttons=None, wheels=None, reportID=None):
        """
        Return an input report for this device.
        :param x: relative x
        :param y: relative y
        :param buttons: a (l, r, m) tuple of bools for the button states,
            where ``None`` is "leave unchanged"
        :param wheels: a single value for the vertical wheel or a (vertical, horizontal) tuple for
            the two wheels
        :param reportID: the numeric report ID for this report, if needed
        """
        if buttons is not None:
            l, r, m = buttons
            if l is not None:
                self.left = l
            if r is not None:
                self.right = r
            if m is not None:
                self.middle = m
        left = self.left
        right = self.right
        middle = self.middle
        wheel, acpan = 0, 0
        if wheels is not None:
            if isinstance(wheels, tuple):
                wheel = wheels[0]
                acpan = wheels[1]
            else:
                wheel = wheels
        reportID = reportID or self.default_reportID
        mouse = MouseData()
        mouse.b1 = int(left)
        mouse.b2 = int(right)
        mouse.b3 = int(middle)
        mouse.x = x
        mouse.y = y
        mouse.wheel = wheel
        mouse.acpan = acpan
        return super().create_report(mouse, reportID=reportID)
    def event(self, x, y, buttons=None, wheels=None):
        """
        Send an input event on the default report ID.
        :param x: relative x
        :param y: relative y
        :param buttons: a (l, r, m) tuple of bools for the button states,
            where ``None`` is "leave unchanged"
        :param wheels: a single value for the vertical wheel or a (vertical, horizontal) tuple for
            the two wheels
        """
        r = self.create_report(x, y, buttons, wheels)
        self.call_input_event(r)
        return [r]
class ButtonMouse(BaseMouse):
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
    def __init__(self, rdesc=report_descriptor, name=None, input_info=None):
        super().__init__(rdesc, name, input_info)
    def fake_report(self, x, y, buttons):
        if buttons is not None:
            left, right, middle = buttons
            if left is None:
                left = self.left
            if right is None:
                right = self.right
            if middle is None:
                middle = self.middle
        else:
            left = self.left
            right = self.right
            middle = self.middle
        button_mask = sum(1 << i for i, b in enumerate([left, right, middle]) if b)
        x = max(-127, min(127, x))
        y = max(-127, min(127, y))
        x = hidtools.util.to_twos_comp(x, 8)
        y = hidtools.util.to_twos_comp(y, 8)
        return [button_mask, x, y]
class WheelMouse(ButtonMouse):
    report_descriptor = [
        0x05, 0x01,  
        0x09, 0x02,  
        0xa1, 0x01,  
        0x05, 0x09,  
        0x19, 0x01,  
        0x29, 0x03,  
        0x15, 0x00,  
        0x25, 0x01,  
        0x95, 0x03,  
        0x75, 0x01,  
        0x81, 0x02,  
        0x95, 0x01,  
        0x75, 0x05,  
        0x81, 0x03,  
        0x05, 0x01,  
        0x09, 0x01,  
        0xa1, 0x00,  
        0x09, 0x30,  
        0x09, 0x31,  
        0x15, 0x81,  
        0x25, 0x7f,  
        0x75, 0x08,  
        0x95, 0x02,  
        0x81, 0x06,  
        0xc0,        
        0x09, 0x38,  
        0x15, 0x81,  
        0x25, 0x7f,  
        0x75, 0x08,  
        0x95, 0x01,  
        0x81, 0x06,  
        0xc0,        
    ]
    def __init__(self, rdesc=report_descriptor, name=None, input_info=None):
        super().__init__(rdesc, name, input_info)
        self.wheel_multiplier = 1
class TwoWheelMouse(WheelMouse):
    report_descriptor = [
        0x05, 0x01,        
        0x09, 0x02,        
        0xa1, 0x01,        
        0x09, 0x01,        
        0xa1, 0x00,        
        0x05, 0x09,        
        0x19, 0x01,        
        0x29, 0x10,        
        0x15, 0x00,        
        0x25, 0x01,        
        0x95, 0x10,        
        0x75, 0x01,        
        0x81, 0x02,        
        0x05, 0x01,        
        0x16, 0x01, 0x80,  
        0x26, 0xff, 0x7f,  
        0x75, 0x10,        
        0x95, 0x02,        
        0x09, 0x30,        
        0x09, 0x31,        
        0x81, 0x06,        
        0x15, 0x81,        
        0x25, 0x7f,        
        0x75, 0x08,        
        0x95, 0x01,        
        0x09, 0x38,        
        0x81, 0x06,        
        0x05, 0x0c,        
        0x0a, 0x38, 0x02,  
        0x95, 0x01,        
        0x81, 0x06,        
        0xc0,              
        0xc0,              
    ]
    def __init__(self, rdesc=report_descriptor, name=None, input_info=None):
        super().__init__(rdesc, name, input_info)
        self.hwheel_multiplier = 1
class MIDongleMIWirelessMouse(TwoWheelMouse):
    report_descriptor = [
        0x05, 0x01,         
        0x09, 0x02,         
        0xa1, 0x01,         
        0x85, 0x01,         
        0x09, 0x01,         
        0xa1, 0x00,         
        0x95, 0x05,         
        0x75, 0x01,         
        0x05, 0x09,         
        0x19, 0x01,         
        0x29, 0x05,         
        0x15, 0x00,         
        0x25, 0x01,         
        0x81, 0x02,         
        0x95, 0x01,         
        0x75, 0x03,         
        0x81, 0x01,         
        0x75, 0x08,         
        0x95, 0x01,         
        0x05, 0x01,         
        0x09, 0x38,         
        0x15, 0x81,         
        0x25, 0x7f,         
        0x81, 0x06,         
        0x05, 0x0c,         
        0x0a, 0x38, 0x02,   
        0x95, 0x01,         
        0x81, 0x06,         
        0xc0,               
        0x85, 0x02,         
        0x09, 0x01,         
        0xa1, 0x00,         
        0x75, 0x0c,         
        0x95, 0x02,         
        0x05, 0x01,         
        0x09, 0x30,         
        0x09, 0x31,         
        0x16, 0x01, 0xf8,   
        0x26, 0xff, 0x07,   
        0x81, 0x06,         
        0xc0,               
        0xc0,               
        0x05, 0x0c,         
        0x09, 0x01,         
        0xa1, 0x01,         
        0x85, 0x03,         
        0x15, 0x00,         
        0x25, 0x01,         
        0x75, 0x01,         
        0x95, 0x01,         
        0x09, 0xcd,         
        0x81, 0x06,         
        0x0a, 0x83, 0x01,   
        0x81, 0x06,         
        0x09, 0xb5,         
        0x81, 0x06,         
        0x09, 0xb6,         
        0x81, 0x06,         
        0x09, 0xea,         
        0x81, 0x06,         
        0x09, 0xe9,         
        0x81, 0x06,         
        0x0a, 0x25, 0x02,   
        0x81, 0x06,         
        0x0a, 0x24, 0x02,   
        0x81, 0x06,         
        0xc0,               
    ]
    device_input_info = (BusType.USB, 0x2717, 0x003B)
    device_name = "uhid test MI Dongle MI Wireless Mouse"
    def __init__(
        self, rdesc=report_descriptor, name=device_name, input_info=device_input_info
    ):
        super().__init__(rdesc, name, input_info)
    def event(self, x, y, buttons=None, wheels=None):
        rs = []
        r = self.create_report(x, y, buttons, wheels, reportID=1)
        self.call_input_event(r)
        rs.append(r)
        r = self.create_report(x, y, buttons, reportID=2)
        self.call_input_event(r)
        rs.append(r)
        return rs
class ResolutionMultiplierMouse(TwoWheelMouse):
    report_descriptor = [
        0x05, 0x01,        
        0x09, 0x02,        
        0xa1, 0x01,        
        0x05, 0x01,        
        0x09, 0x02,        
        0xa1, 0x02,        
        0x85, 0x11,        
        0x09, 0x01,        
        0xa1, 0x00,        
        0x05, 0x09,        
        0x19, 0x01,        
        0x29, 0x03,        
        0x95, 0x03,        
        0x75, 0x01,        
        0x25, 0x01,        
        0x81, 0x02,        
        0x95, 0x01,        
        0x81, 0x01,        
        0x09, 0x05,        
        0x81, 0x02,        
        0x95, 0x03,        
        0x81, 0x01,        
        0x05, 0x01,        
        0x09, 0x30,        
        0x09, 0x31,        
        0x95, 0x02,        
        0x75, 0x08,        
        0x15, 0x81,        
        0x25, 0x7f,        
        0x81, 0x06,        
        0xa1, 0x02,        
        0x85, 0x12,        
        0x09, 0x48,        
        0x95, 0x01,        
        0x75, 0x02,        
        0x15, 0x00,        
        0x25, 0x01,        
        0x35, 0x01,        
        0x45, 0x04,        
        0xb1, 0x02,        
        0x35, 0x00,        
        0x45, 0x00,        
        0x75, 0x06,        
        0xb1, 0x01,        
        0x85, 0x11,        
        0x09, 0x38,        
        0x15, 0x81,        
        0x25, 0x7f,        
        0x75, 0x08,        
        0x81, 0x06,        
        0xc0,              
        0x05, 0x0c,        
        0x75, 0x08,        
        0x0a, 0x38, 0x02,  
        0x81, 0x06,        
        0xc0,              
        0xc0,              
        0xc0,              
    ]
    def __init__(self, rdesc=report_descriptor, name=None, input_info=None):
        super().__init__(rdesc, name, input_info)
        self.default_reportID = 0x11
        self.set_feature_report = [0x12, 0x1]
    def set_report(self, req, rnum, rtype, data):
        if rtype != self.UHID_FEATURE_REPORT:
            raise InvalidHIDCommunication(f"Unexpected report type: {rtype}")
        if rnum != 0x12:
            raise InvalidHIDCommunication(f"Unexpected report number: {rnum}")
        if data != self.set_feature_report:
            raise InvalidHIDCommunication(
                f"Unexpected data: {data}, expected {self.set_feature_report}"
            )
        self.wheel_multiplier = 4
        return 0
class BadResolutionMultiplierMouse(ResolutionMultiplierMouse):
    def set_report(self, req, rnum, rtype, data):
        super().set_report(req, rnum, rtype, data)
        self.wheel_multiplier = 1
        self.hwheel_multiplier = 1
        return 32  
class ResolutionMultiplierHWheelMouse(TwoWheelMouse):
    report_descriptor = [
        0x05, 0x01,         
        0x09, 0x02,         
        0xa1, 0x01,         
        0x05, 0x01,         
        0x09, 0x02,         
        0xa1, 0x02,         
        0x85, 0x1a,         
        0x09, 0x01,         
        0xa1, 0x00,         
        0x05, 0x09,         
        0x19, 0x01,         
        0x29, 0x05,         
        0x95, 0x05,         
        0x75, 0x01,         
        0x15, 0x00,         
        0x25, 0x01,         
        0x81, 0x02,         
        0x75, 0x03,         
        0x95, 0x01,         
        0x81, 0x01,         
        0x05, 0x01,         
        0x09, 0x30,         
        0x09, 0x31,         
        0x95, 0x02,         
        0x75, 0x10,         
        0x16, 0x01, 0x80,   
        0x26, 0xff, 0x7f,   
        0x81, 0x06,         
        0xa1, 0x02,         
        0x85, 0x12,         
        0x09, 0x48,         
        0x95, 0x01,         
        0x75, 0x02,         
        0x15, 0x00,         
        0x25, 0x01,         
        0x35, 0x01,         
        0x45, 0x0c,         
        0xb1, 0x02,         
        0x85, 0x1a,         
        0x09, 0x38,         
        0x35, 0x00,         
        0x45, 0x00,         
        0x95, 0x01,         
        0x75, 0x10,         
        0x16, 0x01, 0x80,   
        0x26, 0xff, 0x7f,   
        0x81, 0x06,         
        0xc0,               
        0xa1, 0x02,         
        0x85, 0x12,         
        0x09, 0x48,         
        0x75, 0x02,         
        0x15, 0x00,         
        0x25, 0x01,         
        0x35, 0x01,         
        0x45, 0x0c,         
        0xb1, 0x02,         
        0x35, 0x00,         
        0x45, 0x00,         
        0x75, 0x04,         
        0xb1, 0x01,         
        0x85, 0x1a,         
        0x05, 0x0c,         
        0x95, 0x01,         
        0x75, 0x10,         
        0x16, 0x01, 0x80,   
        0x26, 0xff, 0x7f,   
        0x0a, 0x38, 0x02,   
        0x81, 0x06,         
        0xc0,               
        0xc0,               
        0xc0,               
        0xc0,               
    ]
    def __init__(self, rdesc=report_descriptor, name=None, input_info=None):
        super().__init__(rdesc, name, input_info)
        self.default_reportID = 0x1A
        self.set_feature_report = [0x12, 0x5]
    def set_report(self, req, rnum, rtype, data):
        super().set_report(req, rnum, rtype, data)
        self.wheel_multiplier = 12
        self.hwheel_multiplier = 12
        return 0
class BaseTest:
    class TestMouse(base.BaseTestCase.TestUhid):
        def test_buttons(self):
            """check for button reliability."""
            uhdev = self.uhdev
            evdev = uhdev.get_evdev()
            syn_event = self.syn_event
            r = uhdev.event(0, 0, (None, True, None))
            expected_event = libevdev.InputEvent(libevdev.EV_KEY.BTN_RIGHT, 1)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEventsIn((syn_event, expected_event), events)
            assert evdev.value[libevdev.EV_KEY.BTN_RIGHT] == 1
            r = uhdev.event(0, 0, (None, False, None))
            expected_event = libevdev.InputEvent(libevdev.EV_KEY.BTN_RIGHT, 0)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEventsIn((syn_event, expected_event), events)
            assert evdev.value[libevdev.EV_KEY.BTN_RIGHT] == 0
            r = uhdev.event(0, 0, (None, None, True))
            expected_event = libevdev.InputEvent(libevdev.EV_KEY.BTN_MIDDLE, 1)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEventsIn((syn_event, expected_event), events)
            assert evdev.value[libevdev.EV_KEY.BTN_MIDDLE] == 1
            r = uhdev.event(0, 0, (None, None, False))
            expected_event = libevdev.InputEvent(libevdev.EV_KEY.BTN_MIDDLE, 0)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEventsIn((syn_event, expected_event), events)
            assert evdev.value[libevdev.EV_KEY.BTN_MIDDLE] == 0
            r = uhdev.event(0, 0, (True, None, None))
            expected_event = libevdev.InputEvent(libevdev.EV_KEY.BTN_LEFT, 1)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEventsIn((syn_event, expected_event), events)
            assert evdev.value[libevdev.EV_KEY.BTN_LEFT] == 1
            r = uhdev.event(0, 0, (False, None, None))
            expected_event = libevdev.InputEvent(libevdev.EV_KEY.BTN_LEFT, 0)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEventsIn((syn_event, expected_event), events)
            assert evdev.value[libevdev.EV_KEY.BTN_LEFT] == 0
            r = uhdev.event(0, 0, (True, True, None))
            expected_event0 = libevdev.InputEvent(libevdev.EV_KEY.BTN_LEFT, 1)
            expected_event1 = libevdev.InputEvent(libevdev.EV_KEY.BTN_RIGHT, 1)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEventsIn(
                (syn_event, expected_event0, expected_event1), events
            )
            assert evdev.value[libevdev.EV_KEY.BTN_RIGHT] == 1
            assert evdev.value[libevdev.EV_KEY.BTN_LEFT] == 1
            r = uhdev.event(0, 0, (False, None, None))
            expected_event = libevdev.InputEvent(libevdev.EV_KEY.BTN_LEFT, 0)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEventsIn((syn_event, expected_event), events)
            assert evdev.value[libevdev.EV_KEY.BTN_RIGHT] == 1
            assert evdev.value[libevdev.EV_KEY.BTN_LEFT] == 0
            r = uhdev.event(0, 0, (None, False, None))
            expected_event = libevdev.InputEvent(libevdev.EV_KEY.BTN_RIGHT, 0)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEventsIn((syn_event, expected_event), events)
            assert evdev.value[libevdev.EV_KEY.BTN_RIGHT] == 0
            assert evdev.value[libevdev.EV_KEY.BTN_LEFT] == 0
        def test_relative(self):
            """Check for relative events."""
            uhdev = self.uhdev
            syn_event = self.syn_event
            r = uhdev.event(0, -1)
            expected_event = libevdev.InputEvent(libevdev.EV_REL.REL_Y, -1)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEvents((syn_event, expected_event), events)
            r = uhdev.event(1, 0)
            expected_event = libevdev.InputEvent(libevdev.EV_REL.REL_X, 1)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEvents((syn_event, expected_event), events)
            r = uhdev.event(-1, 2)
            expected_event0 = libevdev.InputEvent(libevdev.EV_REL.REL_X, -1)
            expected_event1 = libevdev.InputEvent(libevdev.EV_REL.REL_Y, 2)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEvents(
                (syn_event, expected_event0, expected_event1), events
            )
class TestSimpleMouse(BaseTest.TestMouse):
    def create_device(self):
        return ButtonMouse()
    def test_rdesc(self):
        """Check that the testsuite actually manages to format the
        reports according to the report descriptors.
        No kernel device is used here"""
        uhdev = self.uhdev
        event = (0, 0, (None, None, None))
        assert uhdev.fake_report(*event) == uhdev.create_report(*event)
        event = (0, 0, (None, True, None))
        assert uhdev.fake_report(*event) == uhdev.create_report(*event)
        event = (0, 0, (True, True, None))
        assert uhdev.fake_report(*event) == uhdev.create_report(*event)
        event = (0, 0, (False, False, False))
        assert uhdev.fake_report(*event) == uhdev.create_report(*event)
        event = (1, 0, (True, False, True))
        assert uhdev.fake_report(*event) == uhdev.create_report(*event)
        event = (-1, 0, (True, False, True))
        assert uhdev.fake_report(*event) == uhdev.create_report(*event)
        event = (-5, 5, (True, False, True))
        assert uhdev.fake_report(*event) == uhdev.create_report(*event)
        event = (-127, 127, (True, False, True))
        assert uhdev.fake_report(*event) == uhdev.create_report(*event)
        event = (0, -128, (True, False, True))
        with pytest.raises(hidtools.hid.RangeError):
            uhdev.create_report(*event)
class TestWheelMouse(BaseTest.TestMouse):
    def create_device(self):
        return WheelMouse()
    def is_wheel_highres(self, uhdev):
        evdev = uhdev.get_evdev()
        assert evdev.has(libevdev.EV_REL.REL_WHEEL)
        return evdev.has(libevdev.EV_REL.REL_WHEEL_HI_RES)
    def test_wheel(self):
        uhdev = self.uhdev
        high_res_wheel = self.is_wheel_highres(uhdev)
        syn_event = self.syn_event
        mult = uhdev.wheel_multiplier
        r = uhdev.event(0, 0, wheels=1 * mult)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL, 1))
        if high_res_wheel:
            expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL_HI_RES, 120))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
        r = uhdev.event(0, 0, wheels=-1 * mult)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL, -1))
        if high_res_wheel:
            expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL_HI_RES, -120))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
        r = uhdev.event(-1, 2, wheels=3 * mult)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_X, -1))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_Y, 2))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL, 3))
        if high_res_wheel:
            expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL_HI_RES, 360))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
class TestTwoWheelMouse(TestWheelMouse):
    def create_device(self):
        return TwoWheelMouse()
    def is_hwheel_highres(self, uhdev):
        evdev = uhdev.get_evdev()
        assert evdev.has(libevdev.EV_REL.REL_HWHEEL)
        return evdev.has(libevdev.EV_REL.REL_HWHEEL_HI_RES)
    def test_ac_pan(self):
        uhdev = self.uhdev
        high_res_wheel = self.is_wheel_highres(uhdev)
        high_res_hwheel = self.is_hwheel_highres(uhdev)
        assert high_res_wheel == high_res_hwheel
        syn_event = self.syn_event
        hmult = uhdev.hwheel_multiplier
        vmult = uhdev.wheel_multiplier
        r = uhdev.event(0, 0, wheels=(0, 1 * hmult))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL, 1))
        if high_res_hwheel:
            expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL_HI_RES, 120))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
        r = uhdev.event(0, 0, wheels=(0, -1 * hmult))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL, -1))
        if high_res_hwheel:
            expected.append(
                libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL_HI_RES, -120)
            )
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
        r = uhdev.event(-1, 2, wheels=(0, 3 * hmult))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_X, -1))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_Y, 2))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL, 3))
        if high_res_hwheel:
            expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL_HI_RES, 360))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
        r = uhdev.event(-1, 2, wheels=(-3 * vmult, 4 * hmult))
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_X, -1))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_Y, 2))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL, -3))
        if high_res_wheel:
            expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL_HI_RES, -360))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL, 4))
        if high_res_wheel:
            expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL_HI_RES, 480))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
class TestResolutionMultiplierMouse(TestTwoWheelMouse):
    def create_device(self):
        return ResolutionMultiplierMouse()
    def is_wheel_highres(self, uhdev):
        high_res = super().is_wheel_highres(uhdev)
        if not high_res:
            assert uhdev.wheel_multiplier == 1
        return high_res
    def test_resolution_multiplier_wheel(self):
        uhdev = self.uhdev
        if not self.is_wheel_highres(uhdev):
            pytest.skip("Kernel not compatible, we can not trigger the conditions")
        assert uhdev.wheel_multiplier > 1
        assert 120 % uhdev.wheel_multiplier == 0
    def test_wheel_with_multiplier(self):
        uhdev = self.uhdev
        if not self.is_wheel_highres(uhdev):
            pytest.skip("Kernel not compatible, we can not trigger the conditions")
        assert uhdev.wheel_multiplier > 1
        syn_event = self.syn_event
        mult = uhdev.wheel_multiplier
        r = uhdev.event(0, 0, wheels=1)
        expected = [syn_event]
        expected.append(
            libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL_HI_RES, 120 / mult)
        )
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
        r = uhdev.event(0, 0, wheels=-1)
        expected = [syn_event]
        expected.append(
            libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL_HI_RES, -120 / mult)
        )
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_X, 1))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_Y, -2))
        expected.append(
            libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL_HI_RES, 120 / mult)
        )
        for _ in range(mult - 1):
            r = uhdev.event(1, -2, wheels=1)
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEvents(expected, events)
        r = uhdev.event(1, -2, wheels=1)
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_WHEEL, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
class TestBadResolutionMultiplierMouse(TestTwoWheelMouse):
    def create_device(self):
        return BadResolutionMultiplierMouse()
    def is_wheel_highres(self, uhdev):
        high_res = super().is_wheel_highres(uhdev)
        assert uhdev.wheel_multiplier == 1
        return high_res
    def test_resolution_multiplier_wheel(self):
        uhdev = self.uhdev
        assert uhdev.wheel_multiplier == 1
class TestResolutionMultiplierHWheelMouse(TestResolutionMultiplierMouse):
    def create_device(self):
        return ResolutionMultiplierHWheelMouse()
    def is_hwheel_highres(self, uhdev):
        high_res = super().is_hwheel_highres(uhdev)
        if not high_res:
            assert uhdev.hwheel_multiplier == 1
        return high_res
    def test_resolution_multiplier_ac_pan(self):
        uhdev = self.uhdev
        if not self.is_hwheel_highres(uhdev):
            pytest.skip("Kernel not compatible, we can not trigger the conditions")
        assert uhdev.hwheel_multiplier > 1
        assert 120 % uhdev.hwheel_multiplier == 0
    def test_ac_pan_with_multiplier(self):
        uhdev = self.uhdev
        if not self.is_hwheel_highres(uhdev):
            pytest.skip("Kernel not compatible, we can not trigger the conditions")
        assert uhdev.hwheel_multiplier > 1
        syn_event = self.syn_event
        hmult = uhdev.hwheel_multiplier
        r = uhdev.event(0, 0, wheels=(0, 1))
        expected = [syn_event]
        expected.append(
            libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL_HI_RES, 120 / hmult)
        )
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
        r = uhdev.event(0, 0, wheels=(0, -1))
        expected = [syn_event]
        expected.append(
            libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL_HI_RES, -120 / hmult)
        )
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
        expected = [syn_event]
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_X, 1))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_Y, -2))
        expected.append(
            libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL_HI_RES, 120 / hmult)
        )
        for _ in range(hmult - 1):
            r = uhdev.event(1, -2, wheels=(0, 1))
            events = uhdev.next_sync_events()
            self.debug_reports(r, uhdev, events)
            self.assertInputEvents(expected, events)
        r = uhdev.event(1, -2, wheels=(0, 1))
        expected.append(libevdev.InputEvent(libevdev.EV_REL.REL_HWHEEL, 1))
        events = uhdev.next_sync_events()
        self.debug_reports(r, uhdev, events)
        self.assertInputEvents(expected, events)
class TestMiMouse(TestWheelMouse):
    def create_device(self):
        return MIDongleMIWirelessMouse()
    def assertInputEvents(self, expected_events, effective_events):
        remaining = self.assertInputEventsIn(expected_events, effective_events)
        try:
            remaining.remove(libevdev.InputEvent(libevdev.EV_SYN.SYN_REPORT, 0))
        except ValueError:
            pass
        assert remaining == []
