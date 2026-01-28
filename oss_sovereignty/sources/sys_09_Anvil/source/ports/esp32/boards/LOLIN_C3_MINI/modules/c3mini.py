from micropython import const
from machine import Pin
SPI_MOSI = const(4)
SPI_MISO = const(3)
SPI_CLK = const(2)
I2C_SDA = const(8)
I2C_SCL = const(10)
LED = const(7)
BUTTON = const(0)
led = Pin(LED, Pin.OUT, value=0)
button = Pin(BUTTON, Pin.IN, Pin.PULL_UP)
