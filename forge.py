import os
import sys
import json

# --- CONFIG ---
PROJECT_ROOT = os.getcwd()
BUILD_DIR = PROJECT_ROOT + "/build_artifacts"
STAGING_DIR = BUILD_DIR + "/staging"
ROOTFS_DIR = BUILD_DIR + "/rootfs_stage"
TOOLCHAIN_BIN = PROJECT_ROOT + "/ext/toolchain/bin"
GCC = TOOLCHAIN_BIN + "/x86_64-unknown-linux-musl-gcc"
OSS_DIR = PROJECT_ROOT + "/oss_sovereignty"

# --- HELPERS ---

def exists(path):
    try:
        os.stat(path)
        return True
    except OSError:
        return False

def log(msg):
    print("[FORGE] " + msg)

def fail(msg):
    print("[FATAL] " + msg)
    sys.exit(1)

def run(cmd, cwd=None, env_extra=None):
    # Construct Environment
    # We want to inherit current PATH but prepend Toolchain
    current_path = os.getenv("PATH")
    if not current_path: current_path = "/bin:/usr/bin"
    new_path = TOOLCHAIN_BIN + ":" + current_path
    
    # Base Env
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
    ret = os.system(full_cmd)
    if ret != 0:
        fail("Command failed with exit code " + str(ret))

def ensure_dir(path):
    cmd = "mkdir -p " + path
    if os.system(cmd) != 0:
        fail("Could not create dir: " + path)

def load_json(path):
    try:
        with open(path, "r") as f:
            return json.load(f)
    except Exception as e:
        fail("Could not load JSON " + path + ": " + str(e))

def patch_file(path, instruction):
    log("Patching " + path)
    try:
        if "content" in instruction:
            with open(path, "w") as f:
                f.write(instruction["content"])
            return

        with open(path, "r") as f:
            content = f.read()
        
        if "replace" in instruction:
            old = instruction["replace"]
            new = instruction["with"]
            content = content.replace(old, new)
        
        if "prepend" in instruction:
            chunk = instruction["prepend"]
            if chunk not in content:
                content = chunk + content

        if "append" in instruction:
             chunk = instruction["append"]
             if chunk not in content:
                 content = content + chunk
        
        with open(path, "w") as f:
            f.write(content)
            
    except Exception as e:
        log("Patch warning for " + path + ": " + str(e))

# --- STAGES ---

def resolve_vars(s):
    if not isinstance(s, str): return s
    s = s.replace("{rootfs}", ROOTFS_DIR)
    s = s.replace("{staging}", STAGING_DIR)
    return s

def process_package(name):
    def_file = "specs/definitions/" + name + ".json"
    spec = load_json(def_file)
    
    log(">>> FORGING: " + spec["name"] + " <<<")
    
    # 1. Prepare Paths
    # We extract to BUILD_DIR/<name>-src usually
    src_dir_name = spec.get("build_subdir", name + "-src")
    build_cwd = BUILD_DIR + "/" + src_dir_name
    
    # 2. Extract
    # Check if already extracted? For now, we clean and re-extract to be safe/scratch
    archive = spec["archive_name"]
    archive_path = OSS_DIR + "/" + archive
    
    if not exists(archive_path):
        # Try to fetch? The specs might have URL.
        url = spec.get("source_url")
        if url:
            log("Fetching " + url)
            run("wget " + url + " -O " + archive_path)
        else:
            fail("Archive not found and no URL: " + archive)
            
    log("Extracting " + archive)
    if exists(build_cwd):
        run("rm -rf " + build_cwd)
    ensure_dir(build_cwd)
    
    # Tar extract stripping 1 component is standard for most
    strip = spec.get("tar_strip", 1)
    run("tar -xf " + archive_path + " -C " + build_cwd + " --strip-components=" + str(strip))
    
    # 3. Patch
    if "patches" in spec:
        for p in spec["patches"]:
            fpath = build_cwd + "/" + p["file"]
            patch_file(fpath, p)
            
    # Resolve Env
    env_extra = {}
    if "env" in spec:
        for k, v in spec["env"].items():
            env_extra[k] = resolve_vars(v)

    # 4. Configure
    if "configure" in spec:
        run(resolve_vars(spec["configure"]), cwd=build_cwd, env_extra=env_extra)

    # 4b. Post-Configure Patch
    if "post_configure_patches" in spec:
        for p in spec["post_configure_patches"]:
            fpath = build_cwd + "/" + p["file"]
            patch_file(fpath, p)
        
    # 5. Build
    if "build" in spec:
        run(resolve_vars(spec["build"]), cwd=build_cwd, env_extra=env_extra)
        
    # 6. Install
    if "install" in spec:
        run(resolve_vars(spec["install"]), cwd=build_cwd, env_extra=env_extra)

def main():
    log("Initializing Forge...")
    ensure_dir(BUILD_DIR)
    ensure_dir(STAGING_DIR)
    ensure_dir(STAGING_DIR + "/include")
    ensure_dir(STAGING_DIR + "/lib")
    ensure_dir(ROOTFS_DIR)
    
    manifest = load_json("specs/manifest.json")
    for pkg in manifest:
        process_package(pkg)
        
    log("Forge Complete.")

if __name__ == "__main__":
    main()
