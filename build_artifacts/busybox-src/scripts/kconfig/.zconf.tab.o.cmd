cmd_scripts/kconfig/zconf.tab.o := x86_64-unknown-linux-musl-gcc -static -Wp,-MD,scripts/kconfig/.zconf.tab.o.d -Wall -Wstrict-prototypes -O2 -fomit-frame-pointer      -Iscripts/kconfig -c -o scripts/kconfig/zconf.tab.o scripts/kconfig/zconf.tab.c

deps_scripts/kconfig/zconf.tab.o := \
  scripts/kconfig/zconf.tab.c \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdc-predef.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/ctype.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/features.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/alltypes.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/stdarg.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdio.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdlib.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/alloca.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/string.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/strings.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/stdbool.h \
  scripts/kconfig/lkc.h \
  scripts/kconfig/expr.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/libintl.h \
  scripts/kconfig/lkc_proto.h \
  scripts/kconfig/zconf.hash.c \
  scripts/kconfig/lex.zconf.c \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/errno.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/errno.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/inttypes.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/stdint.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdint.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/stdint.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/limits.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/syslimits.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/limits.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/limits.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/unistd.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/posix.h \
  scripts/kconfig/util.c \
  scripts/kconfig/confdata.c \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/stat.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/stat.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/time.h \
  scripts/kconfig/expr.c \
  scripts/kconfig/symbol.c \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/regex.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/sys/utsname.h \
  scripts/kconfig/menu.c \

scripts/kconfig/zconf.tab.o: $(deps_scripts/kconfig/zconf.tab.o)

$(deps_scripts/kconfig/zconf.tab.o):
