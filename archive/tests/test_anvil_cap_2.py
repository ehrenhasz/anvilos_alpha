import os
import anvil

print("Testing os.system...")
try:
    ret = os.system("echo 'System Call Works'")
    print("Return:", ret)
except AttributeError:
    print("os.system missing")
except Exception as e:
    print("Error:", e)
