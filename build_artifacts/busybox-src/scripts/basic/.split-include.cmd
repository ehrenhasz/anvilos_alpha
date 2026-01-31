cmd_scripts/basic/split-include := x86_64-unknown-linux-musl-gcc -static -Wp,-MD,scripts/basic/.split-include.d -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer       -o scripts/basic/split-include scripts/basic/split-include.c  

deps_scripts/basic/split-include := \
  scripts/basic/split-include.c \
    $(wildcard include/config/foo.h) \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdc-predef.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/stat.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/features.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/alltypes.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/stat.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/types.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/endian.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/select.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/ctype.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/errno.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/errno.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/fcntl.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/fcntl.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdio.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdlib.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/alloca.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/string.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/strings.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/unistd.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/posix.h \

scripts/basic/split-include: $(deps_scripts/basic/split-include)

$(deps_scripts/basic/split-include):
