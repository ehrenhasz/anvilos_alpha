from micropython import const
from machine import Pin, I2C, Signal
from s2pico_oled import OLED
SPI_MOSI = const(35)
SPI_MISO = const(36)
SPI_CLK = const(37)
I2C_SDA = const(8)
I2C_SCL = const(9)
DAC1 = const(17)
DAC2 = const(18)
LED = const(10)
OLED_RST = const(18)
BUTTON = const(0)
led = Signal(LED, Pin.OUT, value=0, invert=True)
button = Pin(BUTTON, Pin.IN, Pin.PULL_UP)
i2c = I2C(0)
oled = OLED(i2c, Pin(OLED_RST))
