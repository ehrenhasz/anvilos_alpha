cmd_scripts/basic/docproc := x86_64-unknown-linux-musl-gcc -static -Wp,-MD,scripts/basic/.docproc.d -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer       -o scripts/basic/docproc scripts/basic/docproc.c  

deps_scripts/basic/docproc := \
  scripts/basic/docproc.c \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdc-predef.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdio.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/features.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/alltypes.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdlib.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/alloca.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/string.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/strings.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/ctype.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/unistd.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/posix.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/limits.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/syslimits.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/limits.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/limits.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/types.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/endian.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/select.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/wait.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/signal.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/signal.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/resource.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/time.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/resource.h \

scripts/basic/docproc: $(deps_scripts/basic/docproc)

$(deps_scripts/basic/docproc):
