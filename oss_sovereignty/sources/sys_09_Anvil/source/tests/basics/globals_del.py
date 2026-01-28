"""
1
"""
def _f(): pass
FunctionType = type(_f)
LambdaType = type(lambda: None)
CodeType = None
MappingProxyType = None
SimpleNamespace = None
def _g():
    yield 1
GeneratorType = type(_g())
class _C:
    def _m(self): pass
MethodType = type(_C()._m)
BuiltinFunctionType = type(len)
BuiltinMethodType = type([].append)
del _f
print(str(FunctionType)[:8])
print(str(BuiltinFunctionType)[:8])
