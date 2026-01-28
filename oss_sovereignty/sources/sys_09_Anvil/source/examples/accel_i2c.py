from machine import Pin
from machine import I2C
import time
accel_pwr = Pin("MMA_AVDD")
accel_pwr.value(1)
i2c = I2C(1, baudrate=100000)
addrs = i2c.scan()
print("Scanning devices:", [hex(x) for x in addrs])
if 0x4C not in addrs:
    print("Accelerometer is not detected")
ACCEL_ADDR = 0x4C
ACCEL_AXIS_X_REG = 0
ACCEL_MODE_REG = 7
i2c.mem_write(b"\x01", ACCEL_ADDR, ACCEL_MODE_REG)
print("Try to move accelerometer and watch the values")
while True:
    val = i2c.mem_read(1, ACCEL_ADDR, ACCEL_AXIS_X_REG)
    print(val[0])
    time.sleep(1)
