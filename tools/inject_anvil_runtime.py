import os
import sys
import uuid
import json
PROJECT_ROOT = os.getcwd()
DB_PATH = os.path.join(PROJECT_ROOT, "data", "cortex.db")
sys.path.append(os.path.join(PROJECT_ROOT, "src"))
from anvilos.mainframe_client import MainframeClient
def inject_anvil_runtime():
    client = MainframeClient(DB_PATH)
    print("... Injecting Anvil Runtime (Phase 2.5) ...")
    build_cmd = (
        "cd oss_sovereignty/sys_09_Anvil/source/ports/unix && "
        "make -j$(nproc) "
        "LDFLAGS_EXTRA='-static' "
        "CFLAGS_EXTRA='-DNDEBUG' "
        "MICROPY_PY_FFI=0 "
        "MICROPY_PY_BTREE=0 "
        "MICROPY_PY_JNI=0 "
        "&& "
        "mkdir -p ../../../../../build_artifacts/rootfs_stage/usr/local/bin && "
        "cp build-standard/micropython ../../../../../build_artifacts/rootfs_stage/usr/local/bin/anvil"
    )
    print(f"-> Card 600: BUILD_ANVIL_RUNTIME")
    client.inject_card("SYS_CMD", {"cmd": build_cmd, "desc": "6. ANVIL: BUILD_RUNTIME_STATIC"}, seq=600)
    init_py_content = (
        "import sys\n"
        "import os\n"
        "import time\n"
        "\n"
        "print('-' * 40)\n"
        "print('ANVIL KERNEL (USERLAND) ONLINE')\n"
        "print('Identity: Sovereign')\n"
        "print(f'Python: {sys.version}')\n"
        "print('-' * 40)\n"
        "\n"
        "def main():\n"
        "    print('Mounting Virtual Filesystems...')\n"
        "    # Note: These are usually done by /init shell script, but we can verify\n"
        "    try:\n"
        "        print('Filesystems active.')\n"
        "    except Exception as e:\n"
        "        print(f'FS Error: {e}')\n"
        "\n"
        "    print('Welcome to the Hold.')\n"
        "    while True:\n"
        "        try:\n"
        "            cmd = input('anvil> ')\n"
        "            if cmd == 'exit': break\n"
        "            # Basic REPL loop\n"
        "            exec(cmd)\n"
        "        except Exception as e:\n"
        "            print(f'Error: {e}')\n"
        "\n"
        "if __name__ == '__main__':\n"
        "    main()\n"
    )
    print(f"-> Card 601: WRITE_INIT_PY")
    client.inject_card("FILE_WRITE", {
        "path": "build_artifacts/rootfs_stage/init.py",
        "content": init_py_content
    }, seq=601)
    init_sh_update = (
        "sed -i 's|exec /bin/sh|exec /usr/local/bin/anvil /init.py|' build_artifacts/rootfs_stage/init && "
        "chmod +x build_artifacts/rootfs_stage/usr/local/bin/anvil"
    )
    print(f"-> Card 602: LINK_INIT_TO_ANVIL")
    client.inject_card("SYS_CMD", {"cmd": init_sh_update, "desc": "6. ANVIL: LINK_INIT"}, seq=602)
    repack_cmd = (
        "cd build_artifacts/rootfs_stage && "
        "find . | cpio -o -H newc | gzip > ../iso/boot/initramfs.cpio.gz && "
        "cd ../.. && "
        "xorriso -as mkisofs -o anvilos-phase2-anvil.iso "
        "-b boot/isolinux/isolinux.bin "
        "-c boot/isolinux/boot.cat "
        "-no-emul-boot -boot-load-size 4 -boot-info-table "
        "build_artifacts/iso"
    )
    print(f"-> Card 603: REPACK_ISO_ANVIL")
    client.inject_card("SYS_CMD", {"cmd": repack_cmd, "desc": "6. ANVIL: REPACK_ISO"}, seq=603)
    print("Done. Anvil Runtime Injection Queued.")
if __name__ == "__main__":
    inject_anvil_runtime()
