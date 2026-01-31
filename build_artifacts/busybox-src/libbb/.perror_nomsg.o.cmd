cmd_libbb/perror_nomsg.o := x86_64-unknown-linux-musl-gcc -Wp,-MD,libbb/.perror_nomsg.o.d  -std=gnu99 -Iinclude -Ilibbb  -include include/autoconf.h -D_GNU_SOURCE -DNDEBUG -D_LARGEFILE_SOURCE -D_LARGEFILE64_SOURCE -D_FILE_OFFSET_BITS=64 -DBB_VER='"1.36.1"' -I/home/aimeat/anvilos/build_artifacts/staging/include -static -malign-data=abi -Wall -Wshadow -Wwrite-strings -Wundef -Wstrict-prototypes -Wunused -Wunused-parameter -Wunused-function -Wunused-value -Wmissing-prototypes -Wmissing-declarations -Wno-format-security -Wdeclaration-after-statement -Wold-style-definition -finline-limit=0 -fno-builtin-strlen -fomit-frame-pointer -ffunction-sections -fdata-sections -fno-guess-branch-probability -funsigned-char -static-libgcc -falign-functions=1 -falign-jumps=1 -falign-labels=1 -falign-loops=1 -fno-unwind-tables -fno-asynchronous-unwind-tables -fno-builtin-printf -Oz    -DKBUILD_BASENAME='"perror_nomsg"'  -DKBUILD_MODNAME='"perror_nomsg"' -c -o libbb/perror_nomsg.o libbb/perror_nomsg.c

deps_libbb/perror_nomsg.o := \
  libbb/perror_nomsg.c \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdc-predef.h \
  include/platform.h \
    $(wildcard include/config/werror.h) \
    $(wildcard include/config/big/endian.h) \
    $(wildcard include/config/little/endian.h) \
    $(wildcard include/config/nommu.h) \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/limits.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/syslimits.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/limits.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/features.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/alltypes.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/limits.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/byteswap.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/stdint.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/stdint.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/stdint.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/endian.h \
  /home/aimeat/anvilos/ext/toolchain/lib/gcc/x86_64-unknown-linux-musl/15.2.0/include/stdbool.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/unistd.h \
  /home/aimeat/anvilos/ext/toolchain/x86_64-unknown-linux-musl/sysroot/usr/include/bits/posix.h \

libbb/perror_nomsg.o: $(deps_libbb/perror_nomsg.o)

$(deps_libbb/perror_nomsg.o):
