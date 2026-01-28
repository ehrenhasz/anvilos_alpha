"""
I2C test for the CC3200 based boards.
A MPU-9150 sensor must be connected to the I2C bus.
"""
from machine import I2C
import os
import time
mch = os.uname().machine
if "LaunchPad" in mch:
    i2c_pins = ("GP11", "GP10")
elif "WiPy" in mch:
    i2c_pins = ("GP15", "GP10")
else:
    raise Exception("Board not supported!")
i2c = I2C(0, I2C.MASTER, baudrate=400000)
i2c = I2C()
print(i2c)
i2c = I2C(mode=I2C.MASTER, baudrate=50000, pins=i2c_pins)
print(i2c)
i2c = I2C(0, I2C.MASTER, baudrate=100000)
print(i2c)
i2c = I2C(0, mode=I2C.MASTER, baudrate=400000)
print(i2c)
i2c = I2C(0, mode=I2C.MASTER, baudrate=400000, pins=i2c_pins)
print(i2c)
addr = i2c.scan()[0]
print(addr)
reg = bytearray(1)
reg2 = bytearray(2)
reg2_r = bytearray(2)
reg[0] |= 0x80
print(1 == i2c.writeto_mem(addr, 107, reg))
time.sleep_ms(100)  
print(1 == i2c.readfrom_mem_into(addr, 107, reg))  
print(0x40 == reg[0])
data = i2c.readfrom_mem(addr, 117, 1)  
print(0x68 == data[0])
print(len(data) == 1)
print(1 == i2c.readfrom_mem_into(addr, 117, reg))  
print(0x68 == reg[0])
data = i2c.readfrom_mem(addr, 116, 2)  
print(0x68 == data[1])
print(data == b"\x00\x68")
print(len(data) == 2)
print(2 == i2c.readfrom_mem_into(addr, 116, reg2))  
print(0x68 == reg2[1])
print(reg2 == b"\x00\x68")
print(1 == i2c.readfrom_mem_into(addr, 107, reg))  
print(0x40 == reg[0])
reg[0] = 0
print(1 == i2c.writeto_mem(addr, 107, reg))
i2c.readfrom_mem_into(addr, 107, reg)
print(0 == reg[0])
reg[0] = 0x40
print(1 == i2c.writeto_mem(addr, 107, reg))
i2c.readfrom_mem_into(addr, 107, reg)
print(0x40 == reg[0])
reg[0] |= 0x80
print(1 == i2c.writeto_mem(addr, 107, reg))
time.sleep_ms(100)  
print(2 == i2c.readfrom_mem_into(addr, 107, reg2))
print(0x40 == reg2[0])
print(0x00 == reg2[1])
reg2[0] = 0
reg2[1] |= 0x03
print(2 == i2c.writeto_mem(addr, 107, reg2))
i2c.readfrom_mem_into(addr, 107, reg2_r)
print(reg2 == reg2_r)
reg[0] = 0x80
print(1 == i2c.writeto_mem(addr, 107, reg))
time.sleep_ms(100)  
reg[0] = 117  
print(1 == i2c.writeto(addr, reg, stop=False))  
print(1 == i2c.readfrom_into(addr, reg))
print(reg[0] == 0x68)
reg[0] = 117  
print(1 == i2c.writeto(addr, reg, stop=False))  
print(0x68 == i2c.readfrom(addr, 1)[0])
i2c.readfrom_mem_into(addr, 107, reg2)
print(0x40 == reg2[0])
print(0x00 == reg2[1])
reg2[0] = 107  
reg2[1] = 0
print(2 == i2c.writeto(addr, reg2, stop=True))  
i2c.readfrom_mem_into(addr, 107, reg)  
print(reg[0] == 0)
for i in range(0, 1000):
    i2c = I2C(0, I2C.MASTER, baudrate=100000)
i2c = I2C(0, I2C.MASTER, baudrate=100000)
i2c.deinit()
print(i2c)
try:
    i2c.scan()
except Exception:
    print("Exception")
try:
    i2c.readfrom(addr, 1)
except Exception:
    print("Exception")
try:
    i2c.readfrom_into(addr, reg)
except Exception:
    print("Exception")
try:
    i2c.readfrom_mem_into(addr, 107, reg)
except Exception:
    print("Exception")
try:
    i2c.writeto(addr, reg, stop=False)
except Exception:
    print("Exception")
try:
    i2c.writeto_mem(addr, 107, reg)
except Exception:
    print("Exception")
try:
    i2c.readfrom_mem(addr, 116, 2)
except Exception:
    print("Exception")
try:
    I2C(1, I2C.MASTER, baudrate=100000)
except Exception:
    print("Exception")
i2c.init(baudrate=400000)
print(i2c)
