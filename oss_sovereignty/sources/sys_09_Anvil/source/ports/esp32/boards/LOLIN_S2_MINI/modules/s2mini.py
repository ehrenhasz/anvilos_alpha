from micropython import const
from machine import Pin
SPI_MOSI = const(11)
SPI_MISO = const(9)
SPI_CLK = const(7)
I2C_SDA = const(33)
I2C_SCL = const(35)
DAC1 = const(17)
DAC2 = const(18)
LED = const(15)
BUTTON = const(0)
led = Pin(LED, Pin.OUT, value=0)
button = Pin(BUTTON, Pin.IN, Pin.PULL_UP)
