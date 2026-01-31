import anvil
import os

# --- CONFIG ---
PROJECT_ROOT = os.getcwd()
BUILD_DIR = PROJECT_ROOT + "/build_artifacts"
STAGING_DIR = BUILD_DIR + "/staging"
OSS_DIR = PROJECT_ROOT + "/oss_sovereignty"
TOOLCHAIN_BIN = PROJECT_ROOT + "/ext/toolchain/bin"
GCC = TOOLCHAIN_BIN + "/x86_64-unknown-linux-musl-gcc"

def log(msg):
    print("[ZLIB_FORGE] " + msg)

def run_cmd(cmd, cwd=None):
    path_str = TOOLCHAIN_BIN + ":/usr/bin:/bin"
    env_str = "export PATH='" + path_str + "'; "
    env_str += "export CC='" + GCC + "'; "
    env_str += "export CFLAGS='-I" + STAGING_DIR + "/include -static'; "
    env_str += "export LDFLAGS='-L" + STAGING_DIR + "/lib -static'; "
    
    if cwd:
        env_str += "cd " + cwd + " && "
    
    full_cmd = env_str + cmd
    log("EXEC: " + cmd)
    return anvil.check_output(full_cmd)

def forge_zlib():
    log("Starting Sovereign Zlib Forge...")
    
    # 1. Prep Dirs
    run_cmd("mkdir -p " + STAGING_DIR)
    
    src_dir = BUILD_DIR + "/zlib-src"
    run_cmd("rm -rf " + src_dir)
    run_cmd("mkdir -p " + src_dir)
    
    # 2. Extract
    archive = OSS_DIR + "/zlib-1.3.1.tar.gz"
    run_cmd("tar -xf " + archive + " -C " + src_dir + " --strip-components=1")
    
    # 3. Configure
    run_cmd("./configure --static --prefix=/", cwd=src_dir)
    
    # 4. Build
    run_cmd("make -j4", cwd=src_dir)
    
    # 5. Install
    run_cmd("make install DESTDIR=" + STAGING_DIR, cwd=src_dir)
    
    log("Zlib Forge Complete.")

if __name__ == "__main__":
    forge_zlib()
