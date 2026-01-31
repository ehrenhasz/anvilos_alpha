import sys
import anvil
import os

# --- CONFIG ---
PROJECT_ROOT = os.getcwd() # Assumption: Running from project root
BUILD_DIR = PROJECT_ROOT + "/build_artifacts"
STAGING_DIR = BUILD_DIR + "/staging"
ROOTFS_DIR = BUILD_DIR + "/rootfs_stage"
OSS_DIR = PROJECT_ROOT + "/oss_sovereignty"
TOOLCHAIN_BIN = PROJECT_ROOT + "/ext/toolchain/bin"
GCC = TOOLCHAIN_BIN + "/x86_64-unknown-linux-musl-gcc"

# --- THE LAW (Directives) ---
def get_packages():
    pkgs = []
    
    # 1. ZLIB (Dependency)
    pkgs.append({
        "name": "zlib",
        "archive": "zlib-1.3.1.tar.gz",
        "subdir": "zlib-src",
        "configure": "./configure --static --prefix=/",
        "build": "make -j4",
        "install": "make install DESTDIR={staging}"
    })

    # 2. NCURSES (Dependency for Bash/Vim)
    pkgs.append({
        "name": "ncurses",
        "archive": "ncurses-6.4.tar.gz",
        "subdir": "ncurses-src",
        "configure": "./configure --prefix=/ --with-termlib --with-ticlib --without-progs --enable-static",
        "build": "make -j4",
        "install": "make install DESTDIR={staging}"
    })

    # 3. READLINE (Dependency for Bash)
    pkgs.append({
        "name": "readline",
        "archive": "readline-8.2.tar.gz",
        "subdir": "readline-src",
        "configure": "./configure --prefix=/ --disable-shared --enable-static --host=x86_64-unknown-linux-musl --with-curses",
        "env": { "bash_cv_termcap_lib": "libncurses" },
        "patch_cmd": "sed -i 's/extern int tputs.*/#include <ncurses\/term.h>\\n#undef lines\\n#undef columns\\n#undef newline\\nextern char PC; extern char *BC; extern char *UP; extern short ospeed;/g' config.h tcap.h",
        "build": "make -j4",
        "install": "make install DESTDIR={staging}"
    })

    # 4. OPENSSL (Dependency for SSH)
    pkgs.append({
        "name": "openssl",
        "archive": "openssl-3.2.1.tar.gz",
        "subdir": "openssl-src",
        "configure": "./Configure linux-x86_64 no-shared -static --prefix=/ --libdir=lib --openssldir=/etc/ssl no-async no-tests",
        "build": "make -j4",
        "install": "make install_sw DESTDIR={staging} && make install_sw DESTDIR={rootfs}"
    })

    # 5. COREUTILS (The Basics)
    pkgs.append({
        "name": "coreutils",
        "archive": "coreutils-9.4.tar.xz",
        "subdir": "coreutils-src",
        "env": { "CFLAGS": "-I{staging}/include -static -std=gnu17" },
        "configure": "./configure --prefix=/ --enable-static-link --without-selinux --disable-nls --host=x86_64-unknown-linux-musl",
        "build": "make -j4",
        "install": "make install DESTDIR={rootfs}"
    })

    # 6. BASH (The Shell)
    pkgs.append({
        "name": "bash",
        "archive": "bash-5.2.21.tar.gz",
        "subdir": "bash-src",
        "env": {
            "CFLAGS": "-I{staging}/include -static -std=gnu17 -Wno-error=incompatible-pointer-types -Wno-error=deprecated-non-prototype -Wno-error=strict-prototypes",
            "CC_FOR_BUILD": GCC 
        },
        "configure": "./configure --prefix=/ --enable-static-link --without-bash-malloc --with-installed-readline={staging} --host=x86_64-unknown-linux-musl",
        "patch_cmd": "echo '/* patched */' > lib/sh/strtoimax.c && sed -i '1i#include <unistd.h>' lib/termcap/tparam.c",
        "build": "make -j4",
        "install": "make install DESTDIR={rootfs} && ln -sf bash {rootfs}/bin/sh"
    })

    # 7. VIM (The Editor)
    pkgs.append({
        "name": "vim",
        "archive": "vim-9.1.tar.gz",
        "subdir": "vim-src",
        "env": {
            "vim_cv_tgetent": "yes",
            "vim_cv_terminfo": "yes",
            "vim_cv_terminal_threads": "yes",
            "vim_cv_tty_group": "world",
            "LIBS": "-ltinfo",
            "CFLAGS": "-I{staging}/include -I{staging}/include/ncurses -static",
            "CPPFLAGS": "-I{staging}/include -I{staging}/include/ncurses -static"
        },
        "configure": "./configure --prefix=/ --with-features=tiny --disable-gui --disable-xsmp --disable-nls --host=x86_64-unknown-linux-musl --with-tlib=tinfo",
        "patch_cmd": "sed -i 's/extern int tgetent/\/* extern int tgetent *\//' src/auto/osdef.h && sed -i 's/extern int tputs/\/* extern int tputs *\//' src/auto/osdef.h", 
        "build": "make -j4",
        "install": "make install DESTDIR={rootfs}"
    })
    
    return pkgs

# --- ENGINE ---

def log(msg):
    print("[FORGE] " + msg)

def fail(msg):
    print("[FATAL] " + msg)
    raise Exception("HALT")

def run_cmd(cmd, cwd=None, env_extra=None):
    # Construct Environment
    # Ensure PATH includes our toolchain and system basics
    path_str = TOOLCHAIN_BIN + ":/usr/bin:/bin"
    
    env_str = "export PATH='" + path_str + "'; "
    env_str += "export CC='" + GCC + "'; "
    env_str += "export CFLAGS='-I" + STAGING_DIR + "/include -static'; "
    env_str += "export LDFLAGS='-L" + STAGING_DIR + "/lib -static'; "
    env_str += "export PKG_CONFIG_PATH='" + STAGING_DIR + "/lib/pkgconfig'; "
    
    if env_extra:
        for k in env_extra:
            v = env_extra[k]
            v = v.replace("{staging}", STAGING_DIR)
            v = v.replace("{rootfs}", ROOTFS_DIR)
            env_str += "export " + k + "='" + v + "'; "

    if cwd:
        env_str += "cd " + cwd + " && "
    
    full_cmd = env_str + cmd
    log("EXEC: " + cmd)
    
    try:
        out = anvil.check_output(full_cmd)
    except Exception as e:
        fail("Command failed: " + str(e))

def resolve_vars(s):
    s = s.replace("{rootfs}", ROOTFS_DIR)
    s = s.replace("{staging}", STAGING_DIR)
    return s

def forge():
    log("Initializing Phase 4 Hybrid Forge...")
    
    run_cmd("mkdir -p " + BUILD_DIR)
    run_cmd("mkdir -p " + STAGING_DIR)
    run_cmd("mkdir -p " + STAGING_DIR + "/include")
    run_cmd("mkdir -p " + STAGING_DIR + "/lib")
    run_cmd("mkdir -p " + ROOTFS_DIR)
    
    pkgs = get_packages()
    
    for pkg in pkgs:
        name = pkg["name"]
        log(">>> FORGING: " + name + " <<<")
        
        src_dir = BUILD_DIR + "/" + pkg["subdir"]
        archive = OSS_DIR + "/" + pkg["archive"]
        
        # Clean & Extract
        # Optimization: Don't extract if already configured/built?
        # For now, always clean to be safe as per "start over" instruction.
        run_cmd("rm -rf " + src_dir)
        run_cmd("mkdir -p " + src_dir)
        
        strip_level = "1" 
        run_cmd("tar -xf " + archive + " -C " + src_dir + " --strip-components=" + strip_level)
        
        env_map = {}
        if "env" in pkg:
            env_map = pkg["env"]
            
        if "configure" in pkg:
            cmd = resolve_vars(pkg["configure"])
            run_cmd(cmd, cwd=src_dir, env_extra=env_map)
            
        if "patch_cmd" in pkg:
             cmd = resolve_vars(pkg["patch_cmd"])
             run_cmd(cmd, cwd=src_dir, env_extra=env_map)
             
        if "build" in pkg:
            cmd = resolve_vars(pkg["build"])
            run_cmd(cmd, cwd=src_dir, env_extra=env_map)
            
        if "install" in pkg:
            cmd = resolve_vars(pkg["install"])
            run_cmd(cmd, cwd=src_dir, env_extra=env_map)
            
    log("Forge Complete. Hybrid Sovereignty Achieved.")

if __name__ == "__main__":
    forge()