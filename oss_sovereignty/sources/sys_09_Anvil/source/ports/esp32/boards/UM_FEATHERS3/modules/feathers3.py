from micropython import const
from machine import Pin, ADC
import time
VBUS_SENSE = const(34)
VBAT_SENSE = const(2)
RGB_DATA = const(40)
LDO2 = const(39)
LED = const(13)
AMB_LIGHT = const(4)
SPI_MOSI = const(35)
SPI_MISO = const(37)
SPI_CLK = const(36)
I2C_SDA = const(8)
I2C_SCL = const(9)
def led_set(state):
    """Set the state of the BLUE LED on IO13"""
    l = Pin(LED, Pin.OUT)
    l.value(state)
def led_blink():
    """Toggle the BLUE LED on IO13"""
    l = Pin(LED, Pin.OUT)
    l.value(not l.value())
def get_amb_light():
    """Get Ambient Light Sensor reading"""
    adc = ADC(Pin(AMB_LIGHT))
    adc.atten(ADC.ATTN_11DB)
    return adc.read()
def set_ldo2_power(state):
    """Enable or Disable power to the second LDO"""
    Pin(LDO2, Pin.OUT).value(state)
def get_battery_voltage():
    """
    Returns the current battery voltage. If no battery is connected, returns 4.2V which is the charge voltage
    This is an approximation only, but useful to detect if the charge state of the battery is getting low.
    """
    adc = ADC(Pin(VBAT_SENSE))  
    adc.atten(ADC.ATTN_2_5DB)  
    measuredvbat = adc.read_uv() / 1000000  
    measuredvbat *= 3.7624  
    return round(measuredvbat, 2)
def get_vbus_present():
    """Detect if VBUS (5V) power source is present"""
    return Pin(VBUS_SENSE, Pin.IN).value() == 1
def rgb_color_wheel(wheel_pos):
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
