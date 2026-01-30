cmd_scripts/kconfig/kxgettext.o := /home/aimeat/anvilos/ext/toolchain/bin/x86_64-unknown-linux-musl-gcc -static -Wp,-MD,scripts/kconfig/.kxgettext.o.d -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer       -c -o scripts/kconfig/kxgettext.o scripts/kconfig/kxgettext.c

deps_scripts/kconfig/kxgettext.o := \
  scripts/kconfig/kxgettext.c \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdc-predef.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdlib.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/features.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/alltypes.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/alloca.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/string.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/strings.h \
  scripts/kconfig/lkc.h \
  scripts/kconfig/expr.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdio.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/stdbool.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/libintl.h \
  scripts/kconfig/lkc_proto.h \

scripts/kconfig/kxgettext.o: $(deps_scripts/kconfig/kxgettext.o)

$(deps_scripts/kconfig/kxgettext.o):
