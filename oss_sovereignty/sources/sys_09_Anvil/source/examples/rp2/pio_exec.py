import time
from machine import Pin
import rp2
@rp2.asm_pio(set_init=rp2.PIO.OUT_LOW)
def prog():
    pass
sm = rp2.StateMachine(0, prog, set_base=Pin(25))
sm.exec("set(pins, 1)")
time.sleep(0.5)
sm.exec("set(pins, 0)")
