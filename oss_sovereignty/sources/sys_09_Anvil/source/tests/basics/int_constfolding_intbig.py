print(-0x3fffffff) # 32-bit edge case
print(-0x3fffffffffffffff) # 64-bit edge case
print(-(-0x3fffffff - 1)) # 32-bit edge case
print(-(-0x3fffffffffffffff - 1)) # 64-bit edge case
print(~0x3fffffff) # 32-bit edge case
print(~0x3fffffffffffffff) # 64-bit edge case
print(~(-0x3fffffff - 1)) # 32-bit edge case
print(~(-0x3fffffffffffffff - 1)) # 64-bit edge case
print(1 + ((1 << 65) - (1 << 65)))
print(1 + (-(1 << 65)))
