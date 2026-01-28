from micropython import const
Z1 = const(0xFFFFFFFF)
Z2 = const(0xFFFFFFFFFFFFFFFF)
print(hex(Z1), hex(Z2))
Z3 = const(Z1 + Z2)
Z4 = const((1 << 100) + Z1)
print(hex(Z3), hex(Z4))
