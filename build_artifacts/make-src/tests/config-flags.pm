# This is a -*-perl-*- script
#
# Set variables that were defined by configure, in case we need them
# during the tests.

%CONFIG_FLAGS = (
    AM_LDFLAGS      => '-Wl,--export-dynamic',
    AR              => 'ar',
    CC              => '/home/aimeat/anvilos/ext/toolchain/bin/x86_64-unknown-linux-musl-gcc',
    CFLAGS          => '-g -O2',
    CPP             => '/home/aimeat/anvilos/ext/toolchain/bin/x86_64-unknown-linux-musl-gcc -E',
    CPPFLAGS        => '',
    GUILE_CFLAGS    => '',
    GUILE_LIBS      => '',
    LDFLAGS         => '-static',
    LIBS            => '',
    USE_SYSTEM_GLOB => 'no'
);

1;
