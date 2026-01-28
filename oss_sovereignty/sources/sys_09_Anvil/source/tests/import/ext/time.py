print("time from filesystem")
import sys
_path = sys.path
sys.path = ()
from time import *
sys.path = _path
del _path
extra = 1
