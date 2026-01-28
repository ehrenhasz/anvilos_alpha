from pyb import ADC, Timer
adct = ADC(16)  
print(str(adct)[:19])
adcv = ADC(17)  
print(adcv)
val = adcv.read()
assert val > 1000 and val < 2000
tim = Timer(5, freq=500)
buf = bytearray(b"\xff" * 50)
adcv.read_timed(buf, tim)
print(len(buf))
for i in buf:
    assert i > 50 and i < 150
import array
arv = array.array("h", 25 * [0x7FFF])
adcv.read_timed(arv, tim)
print(len(arv))
for i in arv:
    assert i > 1000 and i < 2000
arv = array.array("i", 30 * [-1])
adcv.read_timed(arv, tim)
print(len(arv))
for i in arv:
    assert i > 1000 and i < 2000
arv = bytearray(b"\xff" * 50)
art = bytearray(b"\xff" * 50)
ADC.read_timed_multi((adcv, adct), (arv, art), tim)
for i in arv:
    assert i > 60 and i < 125
for i in art:
    assert i > 15 and i < 200
arv = array.array("i", 25 * [-1])
art = array.array("i", 25 * [-1])
ADC.read_timed_multi((adcv, adct), (arv, art), tim)
for i in arv:
    assert i > 1000 and i < 2000
for i in art:
    assert i > 50 and i < 2000
arv = array.array("h", 25 * [0x7FFF])
art = array.array("h", 25 * [0x7FFF])
ADC.read_timed_multi((adcv, adct), (arv, art), tim)
for i in arv:
    assert i > 1000 and i < 2000
for i in art:
    assert i > 50 and i < 2000
