try:
    extra_coverage
except NameError:
    print("SKIP")
    raise SystemExit
import errno
import io
data = extra_coverage()
print(data[0], data[1])
print(hash(data[0]))
print(hash(data[1]))
print(hash(bytes(data[0], "utf8")))
print(hash(str(data[1], "utf8")))
stream = data[2]  
stream.set_error(errno.EAGAIN)  
print(stream.read())  
print(stream.read(1))  
print(stream.readline())  
print(stream.readinto(bytearray(10)))  
print(stream.write(b"1"))  
print(stream.write1(b"1"))  
stream.set_buf(b"123")
print(stream.read(4))  
stream.set_buf(b"123")
print(stream.read1(4))  
stream.set_buf(b"123")
print(stream.readline(4))  
try:
    print(stream.ioctl(0, 0))  
except OSError:
    print("OSError")
stream.set_error(0)
print(stream.ioctl(0, bytearray(10)))  
stream2 = data[3]  
print(stream2.read(1))  
stream.set_error(errno.EAGAIN)
buf = io.BufferedWriter(stream, 8)
print(buf.write(bytearray(16)))
print("cpp", extra_cpp_coverage())
import cppexample
print(cppexample.cppfunc(1, 2))
import frzstr1
print(frzstr1.__file__)
import frzmpy1
print(frzmpy1.__file__)
import frzstr_pkg1
print(frzstr_pkg1.__file__, frzstr_pkg1.x)
import frzmpy_pkg1
print(frzmpy_pkg1.__file__, frzmpy_pkg1.x)
from frzstr_pkg2.mod import Foo
print(Foo.x)
from frzmpy_pkg2.mod import Foo
print(Foo.x)
try:
    import frzmpy2
except ZeroDivisionError:
    print("ZeroDivisionError")
import frzmpy3
import frzmpy4
from frzqstr import returns_NULL
print(returns_NULL())
import frozentest
print(frozentest.__file__)
from example_package.foo import bar
print(bar)
bar.f()
import example_package
print(example_package, example_package.foo, example_package.foo.bar)
example_package.f()
example_package.foo.f()
example_package.foo.bar.f()
print(bar == example_package.foo.bar)
from example_package.foo import f as foo_f
foo_f()
print(foo_f == example_package.foo.f)
