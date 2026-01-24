import json
import os
import math
from datetime import datetime

# Best-effort mapping of "The World" to Open Source repositories
REPO_MAP = {
    # --- SYS 12: LANGUAGES ---
    "COBOL": "https://github.com/gnucobol/gnucobol.git",
    "FORTRAN": "https://github.com/gcc-mirror/gcc.git", # Fortran frontend
    "ALGOL": "https://github.com/maroon/algol68g.git",
    "LISP": "https://github.com/sbcl/sbcl.git",
    "BASIC": "https://github.com/chip-basic/chip-basic.git",
    "PASCAL": "https://github.com/fpc/FPCSource.git",
    "ADA": "https://github.com/gcc-mirror/gcc.git", # GNAT
    "SIMULA": "https://github.com/portablesimula/github.io.git", # Limited
    
    "ASM_x86": "https://github.com/nasm/nasm.git",
    "ASM_ARM": "https://github.com/ARM-software/arm-trusted-firmware.git", # Reference
    "ASM_RISCV": "https://github.com/riscv/riscv-isa-sim.git",
    
    "C": "https://github.com/gcc-mirror/gcc.git",
    "CPP": "https://github.com/gcc-mirror/gcc.git",
    "RUST": "https://github.com/rust-lang/rust.git",
    "ZIG": "https://github.com/ziglang/zig.git",
    "GO": "https://github.com/golang/go.git",
    "ODIN": "https://github.com/odin-lang/Odin.git",
    "NIM": "https://github.com/nim-lang/Nim.git",
    "D": "https://github.com/dlang/dmd.git",
    "FORTH": "https://github.com/ForthHub/gforth.git",
    
    "JAVA": "https://github.com/openjdk/jdk.git",
    "C_SHARP": "https://github.com/dotnet/roslyn.git",
    "KOTLIN": "https://github.com/JetBrains/kotlin.git",
    "SCALA": "https://github.com/scala/scala.git",
    "SWIFT": "https://github.com/swiftlang/swift.git",
    "DART": "https://github.com/dart-lang/sdk.git",
    "GROOVY": "https://github.com/apache/groovy.git",
    
    "PYTHON": "https://github.com/python/cpython.git",
    "RUBY": "https://github.com/ruby/ruby.git",
    "PERL": "https://github.com/Perl/perl5.git",
    "LUA": "https://github.com/lua/lua.git",
    "PHP": "https://github.com/php/php-src.git",
    "TCL": "https://github.com/tcltk/tcl.git",
    "JAVASCRIPT": "https://github.com/v8/v8.git",
    "TYPESCRIPT": "https://github.com/microsoft/TypeScript.git",
    "BASH": "https://git.savannah.gnu.org/git/bash.git",
    "POWERSHELL": "https://github.com/PowerShell/PowerShell.git",
    
    "HASKELL": "https://github.com/ghc/ghc.git",
    "ERLANG": "https://github.com/erlang/otp.git",
    "ELIXIR": "https://github.com/elixir-lang/elixir.git",
    "CLOJURE": "https://github.com/clojure/clojure.git",
    "OCAML": "https://github.com/ocaml/ocaml.git",
    "F_SHARP": "https://github.com/dotnet/fsharp.git",
    "ELM": "https://github.com/elm/compiler.git",
    
    "SQL": "https://github.com/postgres/postgres.git", # Reference implementation
    "R": "https://github.com/wch/r-source.git",
    "JULIA": "https://github.com/JuliaLang/julia.git",
    
    "BRAINFUCK": "https://github.com/brain-lang/brainfuck.git",
    "WHITESPACE": "https://github.com/hostilefork/whitespacers.git",

    # --- SYS 13: PLATFORMS ---
    # "z_OS": "https://github.com/hercules-390/hyperion.git", # Emulator as proxy
    "DOS": "https://github.com/microsoft/MS-DOS.git",
    "WINDOWS_3X": "https://github.com/microsoft/winfile.git", # File Manager as proxy
    "WINDOWS_NT": "https://github.com/reactos/reactos.git", # Best OSS proxy
    "OS_2": "https://github.com/psmedley/os2-ports.git", # Proxy
    "AMIGA_OS": "https://github.com/aros-development-team/aros.git", # AROS
    
    "SOLARIS": "https://github.com/illumos/illumos-gate.git",
    "FREEBSD": "https://github.com/freebsd/freebsd-src.git",
    "OPENBSD": "https://github.com/openbsd/src.git",
    "NETBSD": "https://github.com/NetBSD/src.git",
    "DRAGONFLY": "https://github.com/DragonFlyBSD/DragonFlyBSD.git",
    
    "VXWORKS": "https://github.com/Wind-River/vxworks7-layer-for-qemu.git", # Layer proxy
    "QNX": "https://github.com/qnx/qnx-reference.git", # Hypothetical/Limited
    "FREERTOS": "https://github.com/FreeRTOS/FreeRTOS-Kernel.git",
    "ZEPHYR": "https://github.com/zephyrproject-rtos/zephyr.git",
    
    "ANDROID": "https://github.com/aosp-mirror/platform_manifest.git", # Manifest
    
    # --- SYS 14: ACCESSORS ---
    "TN3270": "https://github.com/rbbrittain/x3270.git",
    "TN5250": "https://github.com/tn5250/tn5250.git",
    "X11": "https://github.com/freedesktop/xorg-server.git",
    "WAYLAND": "https://github.com/wayland-project/wayland.git",
    
    "TCP_IP": "https://github.com/lwip-tcpip/lwip.git", # Reference stack
    "DNS": "https://github.com/isc-projects/bind9.git",
    "DHCP": "https://github.com/isc-projects/kea.git",
    
    "GRPC": "https://github.com/grpc/grpc.git",
    "THRIFT": "https://github.com/apache/thrift.git",
    "KAFKA": "https://github.com/apache/kafka.git",
    "MQTT": "https://github.com/eclipse/mosquitto.git",
    "REST": "https://github.com/OAI/OpenAPI-Specification.git", # Spec as source
    "GRAPHQL": "https://github.com/graphql/graphql-js.git"
}

ROOT_DIRS = [
    "oss_sovereignty/sys_12_Languages",
    "oss_sovereignty/sys_13_Platforms",
    "oss_sovereignty/sys_14_Accessors"
]

QUEUE_FILE = "runtime/card_queue.json"

def get_targets():
    targets = []
    print(">> Scanning for un-assimilated worlds...")
    
    for root_path in ROOT_DIRS:
        if not os.path.exists(root_path):
            continue
            
        for category in os.listdir(root_path):
            cat_path = os.path.join(root_path, category)
            if not os.path.isdir(cat_path):
                continue
                
            for item in os.listdir(cat_path):
                # Ignore metadata.json file itself if listed
                if item == "metadata.json": continue
                
                item_path = os.path.join(cat_path, item)
                if not os.path.isdir(item_path):
                    continue
                    
                # Check if url exists in our map
                url = REPO_MAP.get(item)
                
                targets.append({
                    "name": item,
                    "path": item_path,
                    "url": url,
                    "category": category
                })
    return targets

def mint_cards():
    targets = get_targets()
    # Sort for stability
    targets.sort(key=lambda x: x["name"])
    
    # Filter only those with URLs for now (or decide to skip explicitly)
    # The user said "pull down that source", implying we need URLs.
    # We will skip items without URLs in the command construction or explicitly 'echo SKIP'
    
    BATCH_SIZE = 5
    new_cards = []
    
    timestamp = datetime.now().isoformat()
    
    print(f">> Found {len(targets)} potential targets.")
    
    for i in range(0, len(targets), BATCH_SIZE):
        batch = targets[i:i+BATCH_SIZE]
        
        # Build Command
        cmds = []
        names = []
        for t in batch:
            names.append(t["name"])
            if t["url"]:
                # Use assimilate.py
                cmds.append(f"python3 system/scripts/assimilate.py {t['path']} {t['url']}")
            else:
                cmds.append(f"echo 'SKIPPING {t['name']} - NO MAPPED REPO'")
        
        full_cmd = " && ".join(cmds)
        
        card_id = f"sov_world_batch_{i//BATCH_SIZE + 1:03d}"
        
        card = {
            "id": card_id,
            "description": f"WORLD ASSIMILATION BATCH {i//BATCH_SIZE + 1}: {', '.join(names)}",
            "status": "pending",
            "command": full_cmd,
            "created_at": timestamp,
            "batch_details": batch
        }
        new_cards.append(card)

    # Load and Append
    if os.path.exists(QUEUE_FILE):
        with open(QUEUE_FILE, "r") as f:
            queue = json.load(f)
    else:
        queue = []
        
    queue.extend(new_cards)
    
    with open(QUEUE_FILE, "w") as f:
        json.dump(queue, f, indent=2)
        
    print(f">> Minted {len(new_cards)} cards covering {len(targets)} systems.")

if __name__ == "__main__":
    mint_cards()
