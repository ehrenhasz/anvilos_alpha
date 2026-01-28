import time
from hwconfig import LED, BUTTON
while 1:
    LED.value(BUTTON.value())
    time.sleep_ms(10)
