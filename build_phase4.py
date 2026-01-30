#!/usr/bin/env python3
# ANVIL BUILD SCRIPT (PHASE 4 HYBRID)
# This script executes the build process linearly, bypassing the Mainframe/Card system.
# It is written in Python (Anvil Law).

import os
import sys
import subprocess
import shutil

# --- CONFIGURATION ---
PROJECT_ROOT = os.getcwd()
BUILD_DIR = os.path.join(PROJECT_ROOT, "build_artifacts")
STAGING_DIR = os.path.join(BUILD_DIR, "staging") # Headers/Libs for build time
ISO_DIR = os.path.join(BUILD_DIR, "iso")
ROOTFS_DIR = os.path.join(BUILD_DIR, "rootfs_stage")
TOOLCHAIN_BIN = os.path.join(PROJECT_ROOT, "ext", "toolchain", "bin")
GCC = os.path.join(TOOLCHAIN_BIN, "x86_64-unknown-linux-musl-gcc")

# Environment for Toolchain
ENV = os.environ.copy()
ENV["CC"] = GCC
ENV["CFLAGS"] = f"-I{STAGING_DIR}/include -static"
ENV["LDFLAGS"] = f"-L{STAGING_DIR}/lib -static"
ENV["PKG_CONFIG_PATH"] = f"{STAGING_DIR}/lib/pkgconfig"

# Ensure Toolchain
if not os.path.exists(GCC):
    print(f"[CRITICAL] Sovereign Toolchain not found at {GCC}")
    sys.exit(1)

# --- UTILS ---
def run(cmd, cwd=PROJECT_ROOT):
    print(f"\033[1;34m[EXEC] {cmd}\033[0m")
    try:
        subprocess.check_call(cmd, shell=True, cwd=cwd, env=ENV)
    except subprocess.CalledProcessError as e:
        print(f"\033[1;31m[FAIL] {e}\033[0m")
        sys.exit(1)

def fetch(url, dest_name):
    dest_path = os.path.join("oss_sovereignty", dest_name)
    if os.path.exists(dest_path):
        print(f"[SKIP] {dest_name} already exists.")
        return
    print(f"[FETCH] Downloading {url}...")
    run(f"wget {url} -O {dest_path}")

def extract(name, target_dir):
    src = os.path.join("oss_sovereignty", name)
    print(f"[EXTRACT] {name} -> {target_dir}")
    if os.path.exists(target_dir):
        shutil.rmtree(target_dir)
    os.makedirs(target_dir)
    run(f"tar -xf {src} -C {target_dir} --strip-components=1")

# --- BUILD STEPS ---

def step_clean():
    print("--- CLEANING ---")
    if os.path.exists(BUILD_DIR):
        shutil.rmtree(BUILD_DIR)
    os.makedirs(ISO_DIR, exist_ok=True)
    os.makedirs(ROOTFS_DIR, exist_ok=True)
    os.makedirs(STAGING_DIR, exist_ok=True)
    os.makedirs(os.path.join(STAGING_DIR, "include"), exist_ok=True)
    os.makedirs(os.path.join(STAGING_DIR, "lib"), exist_ok=True)

def step_fetch_gnu():
    print("--- FETCHING GNU TOOLS ---")
    fetch("https://ftp.gnu.org/pub/gnu/ncurses/ncurses-6.4.tar.gz", "ncurses-6.4.tar.gz")
    fetch("https://ftp.gnu.org/gnu/readline/readline-8.2.tar.gz", "readline-8.2.tar.gz")
    fetch("https://ftp.gnu.org/gnu/bash/bash-5.2.21.tar.gz", "bash-5.2.21.tar.gz")
    fetch("https://ftp.gnu.org/gnu/coreutils/coreutils-9.4.tar.xz", "coreutils-9.4.tar.xz")
    fetch("https://www.openssl.org/source/openssl-3.2.1.tar.gz", "openssl-3.2.1.tar.gz")

def step_ncurses():
    print("--- FORGING NCURSES ---")
    src_dir = os.path.join(BUILD_DIR, "ncurses-src")
    extract("ncurses-6.4.tar.gz", src_dir)
    run("./configure --prefix=/ --with-termlib --with-ticlib --without-progs --enable-static --host=x86_64-unknown-linux-musl", cwd=src_dir)
    run(f"make -j$(nproc) DESTDIR={STAGING_DIR} install", cwd=src_dir)

def step_readline():
    print("--- FORGING READLINE ---")
    src_dir = os.path.join(BUILD_DIR, "readline-src")
    extract("readline-8.2.tar.gz", src_dir)
    # Force Readline to use ncurses
    env = ENV.copy()
    env["bash_cv_termcap_lib"] = "libncurses"
    run(f"./configure --prefix=/ --disable-shared --enable-static --host=x86_64-unknown-linux-musl --with-curses", cwd=src_dir)
    
    # Fix: Force include of ncurses headers to resolve tputs/tgoto
    # AND undefine conflicting macros that ncurses/term.h defines (like 'lines')
    config_h = os.path.join(src_dir, "config.h")
    if os.path.exists(config_h):
        with open(config_h, "a") as f:
            f.write("\n/* AnvilOS Fix: Include Ncurses Termcap */\n")
            f.write("#include <ncurses/ncurses.h>\n")
            f.write("#include <ncurses/term.h>\n")
            f.write("#undef lines\n")
            f.write("#undef columns\n")
            f.write("#undef newline\n") # Collides in history.c

    # Fix: Overwrite tcap.h to prevent it from redefining tputs/tgoto
    # BUT we must still declare the global variables that terminal.c expects.
    tcap_h = os.path.join(src_dir, "tcap.h")
    with open(tcap_h, "w") as f:
        f.write("/* AnvilOS Fix: Defer to Ncurses but declare globals */\n")
        f.write("#include <ncurses/term.h>\n")
        f.write("#undef lines\n")
        f.write("#undef columns\n")
        f.write("#undef newline\n")
        # Declare legacy termcap globals that readline expects
        f.write("extern char PC;\n")
        f.write("extern char *BC;\n")
        f.write("extern char *UP;\n")
        f.write("extern short ospeed;\n")
            
    run(f"make -j$(nproc) DESTDIR={STAGING_DIR} install", cwd=src_dir)

def step_bash():
    print("--- FORGING BASH ---")
    src_dir = os.path.join(BUILD_DIR, "bash-src")
    extract("bash-5.2.21.tar.gz", src_dir)
    
    # Fix: Patch externs.h to provide correct prototype for list_reverse
    # The compiler treats () as (void) in strict mode, causing 'too many arguments' error.
    externs_h = os.path.join(src_dir, "externs.h")
    if os.path.exists(externs_h):
        with open(externs_h, "r") as f:
            content = f.read()
        content = content.replace("extern GENERIC_LIST *list_reverse ();", "extern GENERIC_LIST *list_reverse PARAMS((GENERIC_LIST *));")
        with open(externs_h, "w") as f:
            f.write(content)

    # Bash needs to know where ncurses/readline are
    # GCC 15 defaults to C23 which breaks old K&R style code (implicit int, etc).
    # Force gnu17 standard.
    env = ENV.copy()
    env['CFLAGS'] += " -std=gnu17"
    # mkbuiltins is compiled with the host compiler. Since we have no host GCC,
    # we use the Sovereign Toolchain (which is x86_64-linux-musl, compatible with x86_64-linux host kernel).
    cc_for_build = f"{GCC} -std=gnu17"
    run(f"./configure --prefix=/ --enable-static-link --without-bash-malloc --with-installed-readline={STAGING_DIR} --host=x86_64-unknown-linux-musl CC_FOR_BUILD='{cc_for_build}'", cwd=src_dir)
    run("make -j$(nproc)", cwd=src_dir)
    # Install to ROOTFS
    os.makedirs(os.path.join(ROOTFS_DIR, "bin"), exist_ok=True)
    shutil.copy(os.path.join(src_dir, "bash"), os.path.join(ROOTFS_DIR, "bin", "bash"))
    run(f"ln -sf bash {os.path.join(ROOTFS_DIR, 'bin', 'sh')}")

def main():
    step_clean()
    step_fetch_gnu()
    step_ncurses()
    step_readline()
    step_bash()
    print("\033[1;32m[DONE] Userland (Bash) Forged Successfully.\033[0m")

if __name__ == "__main__":
    main()