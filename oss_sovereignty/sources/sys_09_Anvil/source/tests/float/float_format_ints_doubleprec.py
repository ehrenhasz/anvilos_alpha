import array
import sys
print("{:.12e}".format(float("9" * 400 + "e-200")))
print("{:.12e}".format(float("9" * 400 + "e-300")))
v1 = 0x54B249AD2594C37D  
v2 = 0x6974E718D7D7625A  
print("{:.12e}".format(array.array("d", v1.to_bytes(8, sys.byteorder))[0]))
print("{:.12e}".format(array.array("d", v2.to_bytes(8, sys.byteorder))[0]))
for i in range(300):
    print(float("1e" + str(i)))
