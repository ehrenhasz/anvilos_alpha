import anvil
import os

# --- CONFIG ---
PROJECT_ROOT = os.getcwd()
BUILD_DIR = PROJECT_ROOT + "/build_artifacts"
STAGING_DIR = BUILD_DIR + "/staging"
ROOTFS_DIR = BUILD_DIR + "/rootfs_stage"
OSS_DIR = PROJECT_ROOT + "/oss_sovereignty"
TOOLCHAIN_BIN = PROJECT_ROOT + "/ext/toolchain/bin"
GCC = TOOLCHAIN_BIN + "/x86_64-unknown-linux-musl-gcc"

def log(msg):
    print("[FORGE] " + msg)

def run_cmd(cmd, cwd=None, env_extra=None):
    path_str = TOOLCHAIN_BIN + ":/usr/bin:/bin"
    
    env_str = "export PATH='" + path_str + "'; "
    env_str += "export CC='" + GCC + "'; "
    env_str += "export CFLAGS='-I" + STAGING_DIR + "/include -static'; "
    env_str += "export LDFLAGS='-L" + STAGING_DIR + "/lib -static'; "
    env_str += "export PKG_CONFIG_PATH='" + STAGING_DIR + "/lib/pkgconfig'; "
    
    if env_extra:
        for k in env_extra:
            v = env_extra[k]
            # Manual replace since mpy might not support dict comprehension fully or cleanly
            v = v.replace("{staging}", STAGING_DIR)
            v = v.replace("{rootfs}", ROOTFS_DIR)
            env_str += "export " + k + "='" + v + "'; "

    if cwd:
        env_str += "cd " + cwd + " && "
    
    full_cmd = env_str + cmd
    log("EXEC: " + cmd)
    # anvil.check_output is the Sovereign Interface
    anvil.check_output(full_cmd)

def forge_all():
    log("Initializing Phase 4 Hybrid Forge (Sovereign)...")
    
    # Setup Dirs
    run_cmd("mkdir -p " + BUILD_DIR)
    run_cmd("mkdir -p " + STAGING_DIR)
    run_cmd("mkdir -p " + STAGING_DIR + "/include")
    run_cmd("mkdir -p " + STAGING_DIR + "/lib")
    run_cmd("mkdir -p " + ROOTFS_DIR)
    
    # --- 1. ZLIB ---
    log(">>> FORGING: ZLIB <<<")
    src = BUILD_DIR + "/zlib-src"
    run_cmd("rm -rf " + src)
    run_cmd("mkdir -p " + src)
    run_cmd("tar -xf " + OSS_DIR + "/zlib-1.3.1.tar.gz -C " + src + " --strip-components=1")
    run_cmd("./configure --static --prefix=/ --libdir=lib", cwd=src)
    run_cmd("make -j4", cwd=src)
    run_cmd("make install DESTDIR=" + STAGING_DIR, cwd=src)

    # --- 2. NCURSES ---
    log(">>> FORGING: NCURSES <<<")
    src = BUILD_DIR + "/ncurses-src"
    run_cmd("rm -rf " + src)
    run_cmd("mkdir -p " + src)
    run_cmd("tar -xf " + OSS_DIR + "/ncurses-6.4.tar.gz -C " + src + " --strip-components=1")
    run_cmd("./configure --prefix=/ --with-termlib --with-ticlib --without-progs --enable-static", cwd=src)
    run_cmd("make -j4", cwd=src)
    run_cmd("make install DESTDIR=" + STAGING_DIR, cwd=src)

    # --- 3. READLINE ---
    log(">>> FORGING: READLINE <<<")
    src = BUILD_DIR + "/readline-src"
    run_cmd("rm -rf " + src)
    run_cmd("mkdir -p " + src)
    run_cmd("tar -xf " + OSS_DIR + "/readline-8.2.tar.gz -C " + src + " --strip-components=1")
    # Patch config.h after configure, but we need to configure first.
    # Note: Readline needs 'bash_cv_termcap_lib' to find ncurses static lib
    env_rl = {"bash_cv_termcap_lib": "libncurses"}
    run_cmd("./configure --prefix=/ --disable-shared --enable-static --host=x86_64-unknown-linux-musl --with-curses", cwd=src, env_extra=env_rl)
    
    # Patch: We use a simple sed to inject headers because Readline < 8.2 has issues with static ncurses symbols
    # Avoid " : " in strings if this was .anv, but this is .py so it is fine.
    # However, strict sed is safer.
    patch = "sed -i 's/extern int tputs.*/#include <ncurses\/term.h>\\n#undef lines\\n#undef columns\\n#undef newline\\nextern char PC; extern char *BC; extern char *UP; extern short ospeed;/g' config.h tcap.h"
    run_cmd(patch, cwd=src)
    
    run_cmd("make -j4", cwd=src)
    run_cmd("make install DESTDIR=" + STAGING_DIR, cwd=src)

    # --- 4. OPENSSL ---
    log(">>> FORGING: OPENSSL <<<")
    src = BUILD_DIR + "/openssl-src"
    run_cmd("rm -rf " + src)
    run_cmd("mkdir -p " + src)
    run_cmd("tar -xf " + OSS_DIR + "/openssl-3.2.1.tar.gz -C " + src + " --strip-components=1")
    run_cmd("./Configure linux-x86_64 no-shared -static --prefix=/ --libdir=lib --openssldir=/etc/ssl no-async no-tests", cwd=src)
    run_cmd("make -j4", cwd=src)
    run_cmd("make install_sw DESTDIR=" + STAGING_DIR, cwd=src)
    # Also install to Rootfs for runtime usage
    run_cmd("make install_sw DESTDIR=" + ROOTFS_DIR, cwd=src)

    # --- 5. COREUTILS ---
    log(">>> FORGING: COREUTILS <<<")
    src = BUILD_DIR + "/coreutils-src"
    run_cmd("rm -rf " + src)
    run_cmd("mkdir -p " + src)
    run_cmd("tar -xf " + OSS_DIR + "/coreutils-9.4.tar.xz -C " + src + " --strip-components=1")
    # Coreutils needs standard CFLAGS
    env_core = {"CFLAGS": "-I{staging}/include -static -std=gnu17"}
    run_cmd("./configure --prefix=/ --enable-static-link --without-selinux --disable-nls --host=x86_64-unknown-linux-musl", cwd=src, env_extra=env_core)
    run_cmd("make -j4", cwd=src)
    run_cmd("make install DESTDIR=" + ROOTFS_DIR, cwd=src)

    # --- 6. BASH ---
    log(">>> FORGING: BASH <<<")
    src = BUILD_DIR + "/bash-src"
    run_cmd("rm -rf " + src)
    run_cmd("mkdir -p " + src)
    run_cmd("tar -xf " + OSS_DIR + "/bash-5.2.21.tar.gz -C " + src + " --strip-components=1")
    
    # Bash needs CC_FOR_BUILD to be set to our Sovereign GCC (native cross)
    env_bash = {
        "CFLAGS": "-I{staging}/include -static -std=gnu17 -Wno-error=incompatible-pointer-types -Wno-error=deprecated-non-prototype -Wno-error=strict-prototypes",
        "CC_FOR_BUILD": GCC 
    }
    run_cmd("./configure --prefix=/ --enable-static-link --without-bash-malloc --with-installed-readline={staging} --host=x86_64-unknown-linux-musl", cwd=src, env_extra=env_bash)
    
    # Patch for static link issues
    run_cmd("echo '/* patched */' > lib/sh/strtoimax.c", cwd=src)
    run_cmd("sed -i '1i#include <unistd.h>' lib/termcap/tcap.c", cwd=src)
    
    run_cmd("make -j4", cwd=src)
    run_cmd("make install DESTDIR=" + ROOTFS_DIR, cwd=src)
    run_cmd("ln -sf bash " + ROOTFS_DIR + "/bin/sh")

    # --- 7. VIM (Tiny) ---
    log(">>> FORGING: VIM <<<")
    src = BUILD_DIR + "/vim-src"
    run_cmd("rm -rf " + src)
    run_cmd("mkdir -p " + src)
    run_cmd("tar -xf " + OSS_DIR + "/vim-9.1.tar.gz -C " + src + " --strip-components=1")
    
    env_vim = {
        "vim_cv_tgetent": "yes",
        "vim_cv_terminfo": "yes",
        "vim_cv_terminal_threads": "yes",
        "vim_cv_tty_group": "world",
        "LIBS": "-ltinfo",
        "CFLAGS": "-I{staging}/include -I{staging}/include/ncurses -static",
        "CPPFLAGS": "-I{staging}/include -I{staging}/include/ncurses -static"
    }
    
    run_cmd("./configure --prefix=/ --with-features=tiny --disable-gui --disable-xsmp --disable-nls --host=x86_64-unknown-linux-musl --with-tlib=tinfo", cwd=src, env_extra=env_vim)
    
    # Patch osdef.h (prototypes)
    # The sed command failed previously because src/auto/osdef.h didn't exist or path wrong.
    # It usually exists AFTER configure.
    # We will try to touch it or ensure it exists? No, configure creates it.
    # Maybe the path 'src/auto/osdef.h' is relative to src_dir.
    # Let's try listing the dir if it fails? 
    # But we want to fix it. 'src/auto/osdef.h' is standard Vim layout.
    # We will attempt the patch.
    patch_vim = "sed -i 's/extern int tgetent/\/* extern int tgetent *\//' src/auto/osdef.h && sed -i 's/extern int tputs/\/* extern int tputs *\//' src/auto/osdef.h"
    run_cmd(patch_vim, cwd=src)
    
    run_cmd("make -j4", cwd=src)
    run_cmd("make install DESTDIR=" + ROOTFS_DIR, cwd=src)

    log("Forge Complete. Hybrid Sovereignty Achieved.")

if __name__ == "__main__":
    forge_all()