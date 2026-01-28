from pyb import Pin, ADCAll
pins = [Pin.cpu.A0, Pin.cpu.A1, Pin.cpu.A2, Pin.cpu.A3]
for p in pins:
    p.init(p.IN)
adc = ADCAll(12)
for p in pins:
    print(p)
for p in pins:
    p.init(p.IN)
adc = ADCAll(12, 0x70003)
for p in pins:
    print(p)
adc = ADCAll(12)
print(adc)
for c in range(19):
    print(type(adc.read_channel(c)))
print(0 < adc.read_core_temp() < 100)
print(0 < adc.read_core_vbat() < 4)
print(0 < adc.read_core_vref() < 2)
print(0 < adc.read_vref() < 4)
