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
stream = data[2]  # has set_error and set_buf. Write always returns error
stream.set_error(errno.EAGAIN)  # non-blocking error
print(stream.read())  # read all encounters non-blocking error
print(stream.read(1))  # read 1 byte encounters non-blocking error
print(stream.readline())  # readline encounters non-blocking error
print(stream.readinto(bytearray(10)))  # readinto encounters non-blocking error
print(stream.write(b"1"))  # write encounters non-blocking error
print(stream.write1(b"1"))  # write1 encounters non-blocking error
stream.set_buf(b"123")
print(stream.read(4))  # read encounters non-blocking error after successful reads
stream.set_buf(b"123")
print(stream.read1(4))  # read1 encounters non-blocking error after successful reads
stream.set_buf(b"123")
print(stream.readline(4))  # readline encounters non-blocking error after successful reads
try:
    print(stream.ioctl(0, 0))  # ioctl encounters non-blocking error; raises OSError
except OSError:
    print("OSError")
stream.set_error(0)
print(stream.ioctl(0, bytearray(10)))  # successful ioctl call
stream2 = data[3]  # is textio
print(stream2.read(1))  # read 1 byte encounters non-blocking error with textio stream
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
