import stm
import pyb
BSRR = 0x18
stm.mem32[stm.GPIOA + BSRR] = 0x00008000
pyb.delay(100)
print(hex(stm.mem32[stm.GPIOA + stm.GPIO_ODR] & 0x00008000))
stm.mem32[stm.GPIOA + BSRR] = 0x80000000
print(hex(stm.mem32[stm.GPIOA + stm.GPIO_ODR] & 0x00008000))
