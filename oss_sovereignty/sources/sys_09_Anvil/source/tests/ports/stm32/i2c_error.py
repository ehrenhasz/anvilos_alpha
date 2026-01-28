import pyb
from pyb import I2C
if not hasattr(pyb, "Accel"):
    print("SKIP")
    raise SystemExit
pyb.Accel()
i2c = I2C(1, I2C.CONTROLLER, dma=True)
pyb.disable_irq()
i2c.mem_read(1, 76, 0x0A)  # should succeed
pyb.enable_irq()
try:
    pyb.disable_irq()
    i2c.mem_read(1, 77, 0x0A)  # should fail
except OSError as e:
    pyb.enable_irq()
    print(repr(e))
i2c.mem_read(1, 76, 0x0A)  # should succeed
pyb.disable_irq()
i2c.mem_write(1, 76, 0x0A)  # should succeed
pyb.enable_irq()
try:
    pyb.disable_irq()
    i2c.mem_write(1, 77, 0x0A)  # should fail
except OSError as e:
    pyb.enable_irq()
    print(repr(e))
i2c.mem_write(1, 76, 0x0A)  # should succeed
i2c.mem_read(1, 76, 0x0A)  # should succeed
try:
    i2c.mem_read(1, 77, 0x0A)  # should fail
except OSError as e:
    print(repr(e))
i2c.mem_read(1, 76, 0x0A)  # should succeed
i2c.mem_write(1, 76, 0x0A)  # should succeed
try:
    i2c.mem_write(1, 77, 0x0A)  # should fail
except OSError as e:
    print(repr(e))
i2c.mem_write(1, 76, 0x0A)  # should succeed
