import sys
import os
import json
import anvil

# --- CONFIG ---
PROJECT_ROOT = os.getcwd()
BUILD_DIR = PROJECT_ROOT + "/build_artifacts"
STAGING_DIR = BUILD_DIR + "/staging"
ROOTFS_DIR = BUILD_DIR + "/rootfs_stage"
OSS_DIR = PROJECT_ROOT + "/oss_sovereignty"
TOOLCHAIN_BIN = PROJECT_ROOT + "/ext/toolchain/bin"
GCC = TOOLCHAIN_BIN + "/x86_64-unknown-linux-musl-gcc"

# --- DEFINITIONS (The Law) ---
PACKAGES = [
    {
        "name": "zlib",
        "archive": "zlib-1.3.1.tar.gz",
        "subdir": "zlib-src",
        "configure": "./configure --static --prefix=/",
        "build": "make -j4",
        "install": "make install DESTDIR={staging}"
    },
    {
        "name": "openssl",
        "archive": "openssl-3.2.1.tar.gz",
        "subdir": "openssl-src",
        "configure": "./Configure linux-x86_64 no-shared -static --prefix=/ --libdir=lib --openssldir=/etc/ssl no-async no-tests",
        "build": "make -j4",
        "install": "make install_sw DESTDIR={staging} && make install_sw DESTDIR={rootfs}"
    },
    {
        "name": "ncurses",
        "archive": "ncurses-6.4.tar.gz",
        "subdir": "ncurses-src",
        "configure": "./configure --prefix=/ --with-termlib --with-ticlib --without-progs --enable-static",
        "build": "make -j4",
        "install": "make install DESTDIR={staging}"
    },
    {
        "name": "readline",
        "archive": "readline-8.2.tar.gz",
        "subdir": "readline-src",
        "configure": "./configure --prefix=/ --disable-shared --enable-static --host=x86_64-unknown-linux-musl --with-curses",
        "env": { "bash_cv_termcap_lib": "libncurses" },
        "post_patches": [
             { "file": "config.h", "append": "\n#include <ncurses/ncurses.h>\n#include <ncurses/term.h>\n#undef lines\n#undef columns\n#undef newline\n" },
             { "file": "tcap.h", "content": "#include <ncurses/term.h>\n#undef lines\n#undef columns\n#undef newline\nextern char PC;\nextern char *BC;\nextern char *UP;\nextern short ospeed;\n" }
        ],
        "build": "make -j4",
        "install": "make install DESTDIR={staging}"
    },
    {
        "name": "bash",
        "archive": "bash-5.2.21.tar.gz",
        "subdir": "bash-src",
        "env": {
            "CFLAGS": "-I{staging}/include -static -std=gnu17 -Wno-error=incompatible-pointer-types -Wno-error=deprecated-non-prototype -Wno-error=strict-prototypes",
            "CC_FOR_BUILD": "x86_64-unknown-linux-musl-gcc -std=gnu17"
        },
        "patches": [
            { "file": "lib/sh/strtoimax.c", "content": "/* Replaced by Anvil Forge */\n" },
            { "file": "lib/termcap/tparam.c", "prepend": "#include <unistd.h>\n" }
        ],
        "configure": "./configure --prefix=/ --enable-static-link --without-bash-malloc --with-installed-readline={staging} --host=x86_64-unknown-linux-musl",
        "build": "make -j4",
        "install": "make install DESTDIR={rootfs} && ln -sf bash {rootfs}/bin/sh"
    },
    {
        "name": "coreutils",
        "archive": "coreutils-9.4.tar.xz",
        "subdir": "coreutils-src",
        "env": { "CFLAGS": "-I{staging}/include -static -std=gnu17" },
        "configure": "./configure --prefix=/ --enable-static-link --without-selinux --disable-nls --host=x86_64-unknown-linux-musl",
        "build": "make -j4",
        "install": "make install DESTDIR={rootfs}"
    },
    {
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
        "post_patches": [
             { "file": "src/auto/osdef.h", "replace": "extern int tgetent", "with": "/* extern int tgetent */" },
             { "file": "src/auto/osdef.h", "replace": "extern int tgetflag", "with": "/* extern int tgetflag */" },
             { "file": "src/auto/osdef.h", "replace": "extern int tgetnum", "with": "/* extern int tgetnum */" },
             { "file": "src/auto/osdef.h", "replace": "extern int tgetstr", "with": "/* extern int tgetstr */" },
             { "file": "src/auto/osdef.h", "replace": "extern char *tgoto", "with": "/* extern char *tgoto */" },
             { "file": "src/auto/osdef.h", "replace": "extern int tputs", "with": "/* extern int tputs */" }
        ],
        "build": "make -j4",
        "install": "make install DESTDIR={rootfs}"
    },
    {
        "name": "openssh",
        "archive": "openssh-9.6p1.tar.gz",
        "subdir": "openssh-src",
        "configure": "./configure --prefix=/ --sysconfdir=/etc/ssh --with-ssl-dir={staging} --with-zlib={staging} --without-zlib-version-check --without-ldns --without-libedit --disable-strip --with-privsep-path=/var/empty --with-xauth=/usr/bin/xauth --host=x86_64-unknown-linux-musl --with-libs='-lz -lpthread' LDFLAGS='-static -pthread' CFLAGS='-static -pthread'",
        "build": "make -j4",
        "install": "make install DESTDIR={rootfs}"
    }
]

# --- ENGINE ---

def log(msg):
    print("[FORGE] " + msg)

def fail(msg):
    print("[FATAL] " + msg)
    sys.exit(1)

def run_cmd(cmd, cwd=None, env_extra=None):
    # Construct Environment
    current_path = os.getenv("PATH")
    if not current_path: current_path = "/bin:/usr/bin"
    new_path = TOOLCHAIN_BIN + ":" + current_path
    
    env_str = "export PATH='" + new_path + "'; "
    env_str += "export CC='" + GCC + "'; "
    env_str += "export CFLAGS='-I" + STAGING_DIR + "/include -static'; "
    env_str += "export LDFLAGS='-L" + STAGING_DIR + "/lib -static'; "
    env_str += "export PKG_CONFIG_PATH='" + STAGING_DIR + "/lib/pkgconfig'; "
    
    if env_extra:
        for k, v in env_extra.items():
            env_str += "export " + k + "='" + v + "'; "

    if cwd:
        env_str += "cd " + cwd + " && "
    
    full_cmd = env_str + cmd
    log("EXEC: " + cmd)
    
    try:
        # Use Anvil's check_output
        out = anvil.check_output(full_cmd)
        print(out) # Print stdout
    except Exception as e:
        fail("Command failed: " + str(e))

def ensure_dir(path):
    run_cmd("mkdir -p " + path)

def resolve_vars(s):
    if not isinstance(s, str): return s
    s = s.replace("{rootfs}", ROOTFS_DIR)
    s = s.replace("{staging}", STAGING_DIR)
    return s

def patch_target(base_dir, instructions):
    if not instructions: return
    for p in instructions:
        fpath = base_dir + "/" + p["file"]
        log("Patching " + fpath)
        
        try:
            if "content" in p:
                with open(fpath, "w") as f:
                    f.write(p["content"])
                continue

            # Read
            with open(fpath, "r") as f:
                content = f.read()
            
            if "replace" in p:
                content = content.replace(p["replace"], p["with"])
            
            if "prepend" in p:
                if p["prepend"] not in content:
                    content = p["prepend"] + content
            
            if "append" in p:
                if p["append"] not in content:
                    content = content + p["append"]
            
            with open(fpath, "w") as f:
                f.write(content)
        except Exception as e:
            log("Patch failed for " + fpath + ": " + str(e))

def forge():
    log("Initializing Anvil Forge...")
    ensure_dir(BUILD_DIR)
    ensure_dir(STAGING_DIR)
    ensure_dir(STAGING_DIR + "/include")
    ensure_dir(STAGING_DIR + "/lib")
    ensure_dir(ROOTFS_DIR)
    
    target = None
    if len(sys.argv) > 1:
        target = sys.argv[1]
        log("Targeting specific package: " + target)
    
    for pkg in PACKAGES:
        if target and pkg["name"] != target:
            continue
            
        log(">>> FORGING: " + pkg["name"] + " <<<")
        
        src_dir = BUILD_DIR + "/" + pkg["subdir"]
        
        # Extract if not exists
        if True: # Always clean for now or check?
            run_cmd("rm -rf " + src_dir)
            run_cmd("mkdir -p " + src_dir)
            archive = OSS_DIR + "/" + pkg["archive"]
            # Extract
            strip = "1" 
            if "openssl" in pkg["name"]:
                strip = "1" # OpenSSL tars are usually root
            
            run_cmd("tar -xf " + archive + " -C " + src_dir + " --strip-components=" + strip)
        
        # Resolve Env
        env_extra = {}
        if "env" in pkg:
            for k, v in pkg["env"].items():
                env_extra[k] = resolve_vars(v)
                
        # Patch?
        if "patches" in pkg:
            patch_target(src_dir, pkg["patches"])

        # Configure
        if "configure" in pkg:
            run_cmd(resolve_vars(pkg["configure"]), cwd=src_dir, env_extra=env_extra)
            
        # Post-Configure Patch (Readline/Vim need this)
        # Note: My script handles both in 'patches' usually, but definitions split them.
        # I'll re-read definitions logic. Some were 'post_configure_patches'.
        # Ah, in my PACKAGES list I just used 'patches'. 
        # Wait, Readline had 'post_configure_patches'.
        # I should separate them or just apply all patches after extract?
        # Readline: config.h exists AFTER configure.
        # So I need a split.
        
        # FIX: I need to split patches in the dicts if strictly following.
        # But 'anvil_forge.py' is the Law now. I can define when they run.
        # Readline needs config.h patch AFTER configure.
        # Vim needs osdef.h patch AFTER configure.
        # So I'll add 'post_patches' key.
        
        if "post_patches" in pkg:
             patch_target(src_dir, pkg["post_patches"])
             
        # Build
        if "build" in pkg:
            run_cmd(resolve_vars(pkg["build"]), cwd=src_dir, env_extra=env_extra)
            
        # Install
        if "install" in pkg:
            run_cmd(resolve_vars(pkg["install"]), cwd=src_dir, env_extra=env_extra)
            
    log("Forge Complete. Sovereign Artifacts Ready.")

if __name__ == "__main__":
    forge()
