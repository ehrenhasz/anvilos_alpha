print(chr(65))
try:
    chr(0x110000)
except ValueError:
    print("ValueError")
