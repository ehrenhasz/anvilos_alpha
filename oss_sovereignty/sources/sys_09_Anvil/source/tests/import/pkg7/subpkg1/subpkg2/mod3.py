from ... import mod1
from ...mod2 import bar
print(mod1.foo)
print(bar)
try:
    from .... import mod1
except ImportError:
    print("ImportError")
