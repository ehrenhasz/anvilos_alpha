import sys
import os
import anvil

# --- CONFIGURATION ---
PROJECT_ROOT = os.getcwd()
BUILD_DIR = PROJECT_ROOT + "/build_artifacts"
STAGING_DIR = BUILD_DIR + "/staging"
ISO_DIR = BUILD_DIR + "/iso"
ROOTFS_DIR = BUILD_DIR + "/rootfs_stage"
TOOLCHAIN_BIN = PROJECT_ROOT + "/ext/toolchain/bin"
GCC = TOOLCHAIN_BIN + "/x86_64-unknown-linux-musl-gcc"

# Envs
ENV_VARS = {
    "CC": GCC,
    "CFLAGS": "-I" + STAGING_DIR + "/include -static",
    "LDFLAGS": "-L" + STAGING_DIR + "/lib -static",
    "PKG_CONFIG_PATH": STAGING_DIR + "/lib/pkgconfig",
    "PATH": os.getenv("PATH")
}

def get_env_prefix():
    s = ""
    for k, v in ENV_VARS.items():
        s += k + "='" + v + "' "
    return s

def run(cmd, cwd=None):
    full_cmd = get_env_prefix()
    if cwd:
        full_cmd += "cd " + cwd + " && "
    full_cmd += cmd
    print("[EXEC]", cmd)
    # os.system streams to stdout/stderr directly and avoids buffering (OOM risk)
    ret = os.system(full_cmd)
    if ret != 0:
        print("[FAIL] Exit Code:", ret)
        sys.exit(1)

def exists(path):
    try:
        # We still use check_output for internal checks where we don't want stdout
        anvil.check_output("[ -e '" + path + "' ]")
        return True
    except OSError:
        return False

def makedirs(path):
    run("mkdir -p " + path)

def rmtree(path):
    run("rm -rf " + path)

def copy(src, dst):
    run("cp -r " + src + " " + dst)

def symlink(target, link_name):
    run("ln -sf " + target + " " + link_name)

def fetch(url, dest_name):
    dest_path = "oss_sovereignty/" + dest_name
    if exists(dest_path):
        print("[SKIP]", dest_name)
        return
    print("[FETCH]", url)
    run("wget " + url + " -O " + dest_path)

def extract(name, target_dir):
    src = "oss_sovereignty/" + name
    print("[EXTRACT]", name, "->", target_dir)
    if exists(target_dir):
        rmtree(target_dir)
    makedirs(target_dir)
    run("tar -xf " + src + " -C " + target_dir + " --strip-components=1")

# --- STEPS ---

def step_clean():
    print("--- CLEANING ---")
    if exists(BUILD_DIR):
        rmtree(BUILD_DIR)
    makedirs(ISO_DIR)
    makedirs(ROOTFS_DIR)
    makedirs(STAGING_DIR)
    makedirs(STAGING_DIR + "/include")
    makedirs(STAGING_DIR + "/lib")

def step_fetch_gnu():
    print("--- FETCHING GNU TOOLS ---")
    fetch("https://ftp.gnu.org/gnu/make/make-4.4.1.tar.gz", "make-4.4.1.tar.gz")
    fetch("https://busybox.net/downloads/busybox-1.36.1.tar.bz2", "busybox-1.36.1.tar.bz2")
    fetch("https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz", "ncurses-6.4.tar.gz")
    fetch("https://ftp.gnu.org/gnu/readline/readline-8.2.tar.gz", "readline-8.2.tar.gz")
    fetch("https://ftp.gnu.org/gnu/bash/bash-5.2.21.tar.gz", "bash-5.2.21.tar.gz")
    fetch("https://ftp.gnu.org/gnu/coreutils/coreutils-9.4.tar.xz", "coreutils-9.4.tar.xz")
    fetch("https://www.openssl.org/source/openssl-3.2.1.tar.gz", "openssl-3.2.1.tar.gz")
    fetch("https://www.zlib.net/zlib-1.3.1.tar.gz", "zlib-1.3.1.tar.gz")
    fetch("https://cdn.openbsd.org/pub/OpenBSD/OpenSSH/portable/openssh-9.6p1.tar.gz", "openssh-9.6p1.tar.gz")
    fetch("https://github.com/vim/vim/archive/v9.1.0000.tar.gz", "vim-9.1.tar.gz")

def step_make():
    print("--- FORGING GNU MAKE ---")
    src_dir = BUILD_DIR + "/make-src"
    extract("make-4.4.1.tar.gz", src_dir)
    
    # Patch lib/fnmatch.c to remove conflicting getenv declaration and ensure stdlib.h
    fnmatch_c = src_dir + "/lib/fnmatch.c"
    if exists(fnmatch_c):
        try:
            with open(fnmatch_c, "r") as f:
                content = f.read()
            content = content.replace("extern char *getenv ();", "/* extern char *getenv (); */")
            if "#include <stdlib.h>" not in content:
                content = "#include <stdlib.h>\n" + content
            with open(fnmatch_c, "w") as f:
                f.write(content)
        except OSError:
            pass

    # Patch src/getopt.c to remove conflicting getenv declaration and ensure stdlib.h
    getopt_c = src_dir + "/src/getopt.c"
    if exists(getopt_c):
        try:
            with open(getopt_c, "r") as f:
                content = f.read()
            content = content.replace("extern char *getenv ();", "/* extern char *getenv (); */")
            if "#include <stdlib.h>" not in content:
                content = "#include <stdlib.h>\n" + content
            with open(getopt_c, "w") as f:
                f.write(content)
        except OSError:
            pass

    # Patch src/getopt.h to remove conflicting getopt declaration
    getopt_h = src_dir + "/src/getopt.h"
    if exists(getopt_h):
        try:
            with open(getopt_h, "r") as f:
                content = f.read()
            content = content.replace("extern int getopt ();", "/* extern int getopt (); */")
            with open(getopt_h, "w") as f:
                f.write(content)
        except OSError:
            pass

    # Build static make using sovereign toolchain
    # We use host make to build sovereign make (Bootstrap)
    run("./configure --prefix=/ --disable-nls --enable-static LDFLAGS='-static' CC='" + GCC + "'", cwd=src_dir)
    run("make -j$(nproc)", cwd=src_dir)
    run("make install DESTDIR=" + ROOTFS_DIR, cwd=src_dir)

def step_busybox():
    print("--- FORGING BUSYBOX (TOOLS) ---")
    src_dir = BUILD_DIR + "/busybox-src"
    extract("busybox-1.36.1.tar.bz2", src_dir)
    
    # Configure busybox for static build
    # We must pass HOSTCC here too, and ensure it builds static binaries for the host
    host_cc_static = GCC + " -static"
    run("make defconfig HOSTCC='" + host_cc_static + "'", cwd=src_dir)
    # Force static
    run("sed -i 's/^# CONFIG_STATIC is not set/CONFIG_STATIC=y/' .config", cwd=src_dir)
    # Disable TC to avoid header issues (common in musl)
    run("sed -i 's/^CONFIG_TC=y/CONFIG_TC=n/' .config", cwd=src_dir)
    
    # Build using Sovereign Toolchain for both HOST and TARGET
    run("make -j$(nproc) CC='" + GCC + "' HOSTCC='" + host_cc_static + "'", cwd=src_dir)
    run("make install CONFIG_PREFIX=" + ROOTFS_DIR + " CC='" + GCC + "' HOSTCC='" + host_cc_static + "'", cwd=src_dir)

def step_ncurses():
    print("--- FORGING NCURSES ---")
    src_dir = BUILD_DIR + "/ncurses-src"
    extract("ncurses-6.4.tar.gz", src_dir)
    # Treat as native build (since we run on x86_64), but enforce static linking so tests run
    cflags = ENV_VARS['CFLAGS']
    run("./configure --prefix=/ --with-termlib --with-ticlib --without-progs --enable-static CC='" + GCC + "' CFLAGS='" + cflags + "'", cwd=src_dir)
    run("make -j$(nproc) DESTDIR=" + STAGING_DIR + " install", cwd=src_dir)

def step_readline():
    print("--- FORGING READLINE ---")
    src_dir = BUILD_DIR + "/readline-src"
    extract("readline-8.2.tar.gz", src_dir)
    
    extra_env = "bash_cv_termcap_lib=libncurses "
    cflags = ENV_VARS['CFLAGS']
    # Pass CFLAGS via CPPFLAGS too as some configures separate them
    run(extra_env + "./configure --prefix=/ --disable-shared --enable-static --host=x86_64-unknown-linux-musl --with-curses CC='" + GCC + "' CFLAGS='" + cflags + "' CPPFLAGS='" + cflags + "'", cwd=src_dir)
    
    config_h = src_dir + "/config.h"
    with open(config_h, "a") as f:
        f.write("\n#include <ncurses/ncurses.h>\n")
        f.write("#include <ncurses/term.h>\n")
        f.write("#undef lines\n")
        f.write("#undef columns\n")
        f.write("#undef newline\n")

    tcap_h = src_dir + "/tcap.h"
    with open(tcap_h, "w") as f:
        f.write("#include <ncurses/term.h>\n")
        f.write("#undef lines\n")
        f.write("#undef columns\n")
        f.write("#undef newline\n")
        f.write("extern char PC;\n")
        f.write("extern char *BC;\n")
        f.write("extern char *UP;\n")
        f.write("extern short ospeed;\n")
            
    run("make -j$(nproc) DESTDIR=" + STAGING_DIR + " install", cwd=src_dir)

def step_bash():
    print("--- FORGING BASH ---")
    src_dir = BUILD_DIR + "/bash-src"
    extract("bash-5.2.21.tar.gz", src_dir)
    
    strtoimax_c = src_dir + "/lib/sh/strtoimax.c"
    if exists(strtoimax_c):
        try:
            with open(strtoimax_c, "r") as f:
                content = f.read()
            if "#if !defined (HAVE_STRTOIMAX)" not in content:
                content = "#include <config.h>\n#if !defined (HAVE_STRTOIMAX)\n" + content + "\n#endif\n"
                with open(strtoimax_c, "w") as f:
                    f.write(content)
        except OSError:
            pass

    # Patch tparam.c for implicit write
    tparam_c = src_dir + "/lib/termcap/tparam.c"
    if exists(tparam_c):
        try:
            with open(tparam_c, "r") as f:
                content = f.read()
            if "#include <unistd.h>" not in content:
                content = "#include <unistd.h>\n" + content
                with open(tparam_c, "w") as f:
                    f.write(content)
        except OSError:
            pass

    strict_flags = " -std=gnu17 -Wno-error=incompatible-pointer-types -Wno-error=deprecated-non-prototype -Wno-error=strict-prototypes"
    cflags_final = ENV_VARS['CFLAGS'] + strict_flags
    cc_build = GCC + " -std=gnu17"
    
    run("./configure --prefix=/ --enable-static-link --without-bash-malloc --with-installed-readline=" + STAGING_DIR + " --host=x86_64-unknown-linux-musl CC='" + GCC + "' CC_FOR_BUILD='" + cc_build + "' CFLAGS='" + cflags_final + "'", cwd=src_dir)
    run("make -j$(nproc) CFLAGS='" + cflags_final + "'", cwd=src_dir)
    
    makedirs(ROOTFS_DIR + "/bin")
    copy(src_dir + "/bash", ROOTFS_DIR + "/bin/bash")
    symlink("bash", ROOTFS_DIR + "/bin/sh")

def step_coreutils():
    print("--- FORGING COREUTILS ---")
    src_dir = BUILD_DIR + "/coreutils-src"
    extract("coreutils-9.4.tar.xz", src_dir)
    
    cflags_final = ENV_VARS['CFLAGS'] + " -std=gnu17"
    run("./configure --prefix=/ --enable-static-link --without-selinux --disable-nls --host=x86_64-unknown-linux-musl CC='" + GCC + "' CFLAGS='" + cflags_final + "'", cwd=src_dir)
    run("make -j$(nproc)", cwd=src_dir)
    run("make install DESTDIR=" + ROOTFS_DIR, cwd=src_dir)

def step_vim():
    print("--- FORGING VIM ---")
    src_dir = BUILD_DIR + "/vim-src"
    extract("vim-9.1.tar.gz", src_dir)
    
    cflags = ENV_VARS['CFLAGS'] + " -I" + STAGING_DIR + "/include/ncurses"
    ldflags = ENV_VARS['LDFLAGS']
    extra = "vim_cv_tgetent=yes vim_cv_terminfo=yes vim_cv_terminal_threads=yes vim_cv_tty_group=world LIBS=-ltinfo "
    run(extra + "./configure --prefix=/ --with-features=tiny --disable-gui --disable-xsmp --disable-nls --host=x86_64-unknown-linux-musl --with-tlib=tinfo CC='" + GCC + "' CFLAGS='" + cflags + "' CPPFLAGS='" + cflags + "' LDFLAGS='" + ldflags + "'", cwd=src_dir)
    
    # Patch auto/osdef.h to remove conflicting prototypes
    osdef_h = src_dir + "/src/auto/osdef.h"
    if exists(osdef_h):
        try:
            with open(osdef_h, "r") as f:
                lines = f.readlines()
            with open(osdef_h, "w") as f:
                for line in lines:
                    if "tgetent" in line or "tgetflag" in line or "tgetnum" in line or "tputs" in line or "tgoto" in line:
                        f.write("/* " + line.strip() + " */\n")
                    else:
                        f.write(line)
        except OSError:
            pass

    run("make -j$(nproc)", cwd=src_dir)
    run("make install DESTDIR=" + ROOTFS_DIR, cwd=src_dir)

def step_openssl():
    print("--- FORGING OPENSSL ---")
    src_dir = BUILD_DIR + "/openssl-src"
    extract("openssl-3.2.1.tar.gz", src_dir)
    
    run("./Configure linux-x86_64 no-shared -static --prefix=/ --openssldir=/etc/ssl no-async no-tests", cwd=src_dir)
    run("make -j$(nproc) CC='" + GCC + "'", cwd=src_dir)
    run("make install_sw DESTDIR=" + STAGING_DIR, cwd=src_dir)
    run("make install_sw DESTDIR=" + ROOTFS_DIR, cwd=src_dir)

def step_zlib():
    print("--- FORGING ZLIB ---")
    src_dir = BUILD_DIR + "/zlib-src"
    extract("zlib-1.3.1.tar.gz", src_dir)
    
    # Zlib configure is sensitive. Clear CFLAGS for this step, let it handle optimization.
    # We pass --static via configure.
    env_override = "CC='" + GCC + "' CFLAGS='' " 
    run(env_override + "./configure --static --prefix=/", cwd=src_dir)
    run("make -j$(nproc)", cwd=src_dir)
    run("make install DESTDIR=" + STAGING_DIR, cwd=src_dir)

def step_openssh():
    print("--- FORGING OPENSSH ---")
    src_dir = BUILD_DIR + "/openssh-src"
    extract("openssh-9.6p1.tar.gz", src_dir)
    
    cflags = ENV_VARS['CFLAGS']
    ldflags = ENV_VARS['LDFLAGS'] + " -pthread"
    
    # OpenSSH needs to find zlib and openssl in staging
    # We static link everything.
    # --without-openssl-header-check might be needed if cross-compiling logic is strict
    run("./configure --prefix=/ --sysconfdir=/etc/ssh --with-ssl-dir=" + STAGING_DIR + " --with-zlib=" + STAGING_DIR + " --without-pam --without-selinux --with-privsep-path=/var/empty --enable-static --host=x86_64-unknown-linux-musl CC='" + GCC + "' CFLAGS='" + cflags + "' LDFLAGS='" + ldflags + "'", cwd=src_dir)
    
    run("make -j$(nproc)", cwd=src_dir)
    run("make install DESTDIR=" + ROOTFS_DIR, cwd=src_dir)
    
    # Create privilege separation directory
    makedirs(ROOTFS_DIR + "/var/empty")

def main():
    # step_clean()
    step_fetch_gnu()
    
    # 1. Forge the Build Tools (Bootstrap)
    # step_make()
    # step_busybox()
    
    # 2. Switch to Sovereign Tools
    print("--- SWITCHING TO SOVEREIGN TOOLS ---")
    ENV_VARS["PATH"] = TOOLCHAIN_BIN + ":" + ROOTFS_DIR + "/bin:" + ROOTFS_DIR + "/sbin:" + os.getenv("PATH")
    print("[INFO] PATH is now:", ENV_VARS["PATH"])
    
    # 3. Forge the System
    # step_ncurses()
    # step_readline()
    # step_bash()
    # step_coreutils()
    # step_vim()
    # step_openssl()
    
    # 4. Forge Network Services
    step_zlib()
    step_openssh()
    
    print("[DONE] Phase 4 (SSH) Forged Successfully via Anvil.")

if __name__ == "__main__":
    main()
