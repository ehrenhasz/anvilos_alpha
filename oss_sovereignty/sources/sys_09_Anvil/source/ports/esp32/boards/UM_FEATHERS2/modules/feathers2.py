from micropython import const
from machine import Pin, SPI, ADC
import machine, time
LDO2 = const(21)
DOTSTAR_CLK = const(45)
DOTSTAR_DATA = const(40)
SPI_MOSI = const(35)
SPI_MISO = const(37)
SPI_CLK = const(36)
I2C_SDA = const(8)
I2C_SCL = const(9)
DAC1 = const(17)
DAC2 = const(18)
LED = const(13)
AMB_LIGHT = const(4)
def set_led(state):
    l = Pin(LED, Pin.OUT)
    l.value(state)
def toggle_led(state):
    l = Pin(LED, Pin.OUT)
    l.value(not l.value())
def get_amb_light():
    adc = ADC(Pin(AMB_LIGHT))
    adc.atten(ADC.ATTN_11DB)
    return adc.read()
def set_ldo2_power(state):
    """Set the power for the on-board Dotstar to allow no current draw when not needed."""
    ldo2 = Pin(LDO2, Pin.OUT)
    ldo2.value(state)
    if state:
        Pin(DOTSTAR_CLK, Pin.OUT)
        Pin(DOTSTAR_DATA, Pin.OUT)  # If power is on, set CLK to be output, otherwise input
    else:
        Pin(DOTSTAR_CLK, Pin.IN)
        Pin(DOTSTAR_DATA, Pin.IN)  # If power is on, set CLK to be output, otherwise input
    time.sleep(0.035)
def dotstar_color_wheel(wheel_pos):
    """Color wheel to allow for cycling through the rainbow of RGB colors."""
    wheel_pos = wheel_pos % 255
    if wheel_pos < 85:
        return 255 - wheel_pos * 3, 0, wheel_pos * 3
    elif wheel_pos < 170:
        wheel_pos -= 85
        return 0, wheel_pos * 3, 255 - wheel_pos * 3
    else:
        wheel_pos -= 170
        return wheel_pos * 3, 255 - wheel_pos * 3, 0
def go_deepsleep(t):
    """Deep sleep helper that also powers down the on-board Dotstar."""
    set_ldo2_power(False)
    machine.deepsleep(t)
