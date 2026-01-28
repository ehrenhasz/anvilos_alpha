import sys
print(sys, hasattr(sys, "__file__"))
sys.path.clear()
sys.path.append("ext")
import micropython
print(micropython, hasattr(micropython, "__file__"))
import sys
print(sys, hasattr(sys, "__file__"))
import usys
print(usys, hasattr(usys, "__file__"))
import os
print(os, hasattr(os, "__file__"), os.sep, os.extra)
import time
print(time, hasattr(time, "__file__"), time.sleep, time.extra)
import uos
print(uos, hasattr(uos, "__file__"), hasattr(uos, "extra"))
import utime
print(utime, hasattr(utime, "__file__"), hasattr(utime, "extra"))
