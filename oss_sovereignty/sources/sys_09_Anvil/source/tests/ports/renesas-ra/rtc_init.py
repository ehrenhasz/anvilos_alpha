import machine
from machine import RTC
import time
rtc = RTC()
rtc.init()
print(rtc)
rtc.datetime((2014, 1, 1, 1, 0, 0, 0, 0))
time.sleep_ms(1002)
print(rtc.datetime()[:7])
