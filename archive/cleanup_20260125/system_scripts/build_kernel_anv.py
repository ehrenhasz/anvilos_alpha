#!/usr/bin/env python3
import os
import subprocess
import glob

PROJECT_ROOT = os.path.abspath(os.getcwd())
ANVIL_PY = os.path.join(PROJECT_ROOT, "oss_sovereignty", "sys_09_Anvil", "source", "anvil.py")
GCC = os.path.join(PROJECT_ROOT, "ext", "toolchain", "bin", "x86_64-unknown-linux-musl-gcc")
KERNEL_ROOT = os.path.join(PROJECT_ROOT, "oss_sovereignty", "sys_01_Linux_Kernel", "source")
INCLUDE_DIR = os.path.join(KERNEL_ROOT, "include")

def main():
    print("--- ANVIL KERNEL BUILDER ---")
    
    # Find all .anv files
    anv_files = glob.glob(os.path.join(KERNEL_ROOT, "**", "*.anv"), recursive=True)
    
    # 1. Generate Header
    print("[HEADER] Generating kernel.h...")
    if not os.path.exists(INCLUDE_DIR):
        os.makedirs(INCLUDE_DIR)
    
    header_path = os.path.join(INCLUDE_DIR, "kernel.h")
    cmd_header = ["python3", ANVIL_PY, "header", header_path] + anv_files
    res_header = subprocess.run(cmd_header, capture_output=True, text=True)
    
    if res_header.returncode != 0:
        print(f"  [FAIL] Header generation failed:\n{res_header.stderr}")
        return
    else:
        print(f"  [OK] Generated {header_path}")

    # 2. Build Files
    success_count = 0
    fail_count = 0
    
    for anv in anv_files:
        if "man_pager.anv" in anv: continue # Skip userland tool
        
        c_file = anv.replace(".anv", ".c")
        o_file = anv.replace(".anv", ".o")
        
        print(f"[BUILD] {os.path.relpath(anv, PROJECT_ROOT)}")
        
        # Transpile
        cmd_trans = ["python3", ANVIL_PY, "transpile", anv, c_file]
        res_trans = subprocess.run(cmd_trans, capture_output=True, text=True)
        
        if res_trans.returncode != 0:
            print(f"  [FAIL] Transpilation failed:\n{res_trans.stderr}")
            fail_count += 1
            continue
            
        # Compile
        # -std=gnu99 for compound literals and asm support
        # -Wno-implicit-function-declaration because prototypes might be missing in some order or recursion
        cmd_comp = [GCC, "-c", c_file, "-o", o_file, "-I", INCLUDE_DIR, "-std=gnu99", "-Wno-implicit-function-declaration", "-Wno-int-conversion"]
        res_comp = subprocess.run(cmd_comp, capture_output=True, text=True)
        
        if res_comp.returncode != 0:
            print(f"  [FAIL] Compilation failed:\n{res_comp.stderr}")
            fail_count += 1
        else:
            print(f"  [OK] Built {os.path.basename(o_file)}")
            success_count += 1

    print(f"--- BUILD SUMMARY: {success_count} OK, {fail_count} FAILED ---")

    # 3. Compile Assembly (Minimal Set)
    asm_files = [
        os.path.join(KERNEL_ROOT, "arch/x86/boot/header.S"),
        os.path.join(KERNEL_ROOT, "kernel/sched/switch.S")
    ]
    
    final_objs = []
    
    # Collect .anv objects
    for anv in anv_files:
        if "man_pager.anv" in anv: continue
        o_file = anv.replace(".anv", ".o")
        if os.path.exists(o_file):
            final_objs.append(o_file)

    for asm in asm_files:
        o_file = asm.replace(".S", ".o")
        print(f"[ASM] {os.path.relpath(asm, PROJECT_ROOT)}")
        cmd_asm = [GCC, "-c", asm, "-o", o_file, "-I", INCLUDE_DIR]
        res_asm = subprocess.run(cmd_asm, capture_output=True, text=True)
        if res_asm.returncode != 0:
            print(f"  [FAIL] ASM Compilation failed:\n{res_asm.stderr}")
            fail_count += 1
        else:
            print(f"  [OK] Built {os.path.basename(o_file)}")
            final_objs.append(o_file)
            success_count += 1

    # 4. Link
    print("[LINK] Linking kernel.bin...")
    linker_script = os.path.join(KERNEL_ROOT, "linker.ld")
    
    if not os.path.exists(linker_script):
        print(f"  [FAIL] Linker script not found: {linker_script}")
        return

    cmd_link = [GCC, "-T", linker_script, "-o", "kernel.bin", "-ffreestanding", "-O2", "-nostdlib", "-z", "max-page-size=0x1000"] + final_objs + ["-lgcc"]
    
    res_link = subprocess.run(cmd_link, capture_output=True, text=True)
    if res_link.returncode != 0:
        print(f"  [FAIL] Linking failed:\n{res_link.stderr}")
    else:
        print(f"  [OK] Kernel Linked: kernel.bin")

if __name__ == "__main__":
    main()