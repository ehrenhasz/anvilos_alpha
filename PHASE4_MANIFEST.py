# PHASE 4: HYBRID SOVEREIGNTY (MANIFEST)
# STRICT MINIMALISM: English, x86_64, GNU Tools, SSH, Vim, ZFS.

COMPONENTS = [
    # 1. The Substrate
    {
        "name": "linux-kernel",
        "version": "6.6.14",
        "source": "oss_sovereignty/linux-6.6.14.tar.xz",
        "config": "tiny_defconfig",
        "desc": "Kernel"
    },
    
    # 2. The Filesystem (The Hold)
    {
        "name": "zfs",
        "version": "2.2.2",
        "source": "oss_sovereignty/zfs-2.2.2.tar.gz",
        "flags": "--enable-static --disable-systemd --disable-pyzfs",
        "desc": "ZFS on Linux (Static)"
    },
    {
        "name": "zlib",
        "version": "1.3.1",
        "source": "oss_sovereignty/zlib-1.3.1.tar.gz",
        "desc": "ZFS Dependency"
    },
    {
        "name": "util-linux",
        "version": "2.39.3",
        "source": "oss_sovereignty/util-linux-2.39.3.tar.gz",
        "flags": "--disable-all-programs --enable-libuuid --enable-libblkid",
        "desc": "ZFS Dependencies (libuuid, libblkid)"
    },

    # 3. The Shell & Core
    {
        "name": "ncurses",
        "version": "6.4",
        "source": "https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz",
        "flags": "--with-termlib --with-ticlib --without-progs --enable-static",
        "desc": "Dependency for Bash/Vim"
    },
    {
        "name": "readline",
        "version": "8.2",
        "source": "https://ftp.gnu.org/gnu/readline/readline-8.2.tar.gz",
        "flags": "--disable-shared --enable-static",
        "desc": "Dependency for Bash"
    },
    {
        "name": "bash",
        "version": "5.2.21",
        "source": "https://ftp.gnu.org/gnu/bash/bash-5.2.21.tar.gz",
        "flags": "--enable-static-link --without-bash-malloc",
        "desc": "The Shell (No Busybox)"
    },
    {
        "name": "coreutils",
        "version": "9.4",
        "source": "https://ftp.gnu.org/gnu/coreutils/coreutils-9.4.tar.xz",
        "flags": "--enable-no-install-program=stdbuf,timeout,nice ...", 
        "desc": "Standard GNU Tools (ls, cp, mv...)"
    },

    # 4. The Editor
    {
        "name": "vim",
        "version": "9.1",
        "source": "oss_sovereignty/vim-9.1.tar.gz",
        "flags": "--with-features=tiny --disable-gui --disable-xsmp",
        "desc": "Text Editor"
    },

    # 5. The Network
    {
        "name": "openssl",
        "version": "3.2.1",
        "source": "https://www.openssl.org/source/openssl-3.2.1.tar.gz",
        "desc": "Crypto for SSH"
    },
    {
        "name": "openssh",
        "version": "9.6p1",
        "source": "oss_sovereignty/openssh-9.6p1.tar.gz",
        "flags": "--with-ssl-dir=... --with-libs='-lz -ldl' --static",
        "desc": "SSH Client/Server"
    },
    
    # 6. The Bridge (Anvil Runtime)
    {
        "name": "anvil_runtime",
        "type": "binary",
        "source": "local_build",
        "desc": "MicroPython-based Userspace Runtime"
    }
]