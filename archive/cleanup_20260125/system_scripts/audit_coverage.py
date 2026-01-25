import json
import os
import sys

# Import the data structures directly from the scripts
# (We'll just copy the relevant dicts here to avoid import issues with the scripts being standalone)

SYSTEMS = {
    "sys_12_Languages": {
        "categories": {
            "01_Ancient": ["COBOL", "FORTRAN", "ALGOL", "PL_I", "LISP", "BASIC", "PASCAL", "ADA", "SIMULA"],
            "02_Assembly": ["ASM_x86", "ASM_ARM", "ASM_RISCV", "ASM_zARCH", "ASM_6502", "ASM_Z80"],
            "03_System": ["C", "CPP", "RUST", "ZIG", "GO", "ODIN", "NIM", "D", "FORTH"],
            "04_Managed": ["JAVA", "C_SHARP", "KOTLIN", "SCALA", "SWIFT", "DART", "GROOVY"],
            "05_Scripting": ["PYTHON", "RUBY", "PERL", "LUA", "PHP", "TCL", "JAVASCRIPT", "TYPESCRIPT", "BASH", "POWERSHELL"],
            "06_Functional": ["HASKELL", "ERLANG", "ELIXIR", "CLOJURE", "OCAML", "F_SHARP", "SCHEME", "RACKET", "ELM"],
            "07_Data": ["SQL", "R", "JULIA", "MATLAB", "SAS", "WOLFRAM"],
            "08_Esoteric": ["BRAINFUCK", "WHITESPACE", "BEFUNGE", "MALBOLGE"]
        }
    },
    "sys_13_Platforms": {
        "categories": {
            "01_IBM_Mainframe": ["z_OS", "z_VM", "z_VSE", "TPF", "OS_400", "AIX"],
            "02_Unisys": ["OS_2200", "MCP"],
            "03_DEC": ["VMS", "TOPS_10", "TOPS_20", "ULTRIX", "TRU64"],
            "04_Unix_Commercial": ["SOLARIS", "HP_UX", "IRIX", "SCO_OPENSERVER"],
            "05_Unix_BSD": ["FREEBSD", "OPENBSD", "NETBSD", "DRAGONFLY"],
            "06_Microcomputer": ["DOS", "WINDOWS_3X", "WINDOWS_NT", "WINDOWS_9X", "OS_2", "AMIGA_OS"],
            "07_Embedded": ["VXWORKS", "QNX", "FREERTOS", "ZEPHYR"],
            "08_Mobile": ["ANDROID", "IOS", "SYMBIAN", "PALM_OS", "BLACKBERRY"]
        }
    },
    "sys_14_Accessors": {
        "categories": {
            "01_Terminals": ["TN3270", "TN5250", "VT100", "VT220", "ANSI", "X11", "WAYLAND"],
            "02_Network_Stack": ["TCP_IP", "UDP", "SCTP", "ICMP", "IGMP", "ARP", "DNS", "DHCP"],
            "03_Legacy_Net": ["SNA", "DECNET", "IPX_SPX", "APPLETALK", "X_25", "FRAME_RELAY"],
            "04_RPC_Messaging": ["GRPC", "THRIFT", "CORBA", "SOAP", "REST", "GRAPHQL", "MQTT", "AMQP", "KAFKA"],
            "05_Hardware_Bus": ["USB", "PCIE", "I2C", "SPI", "CAN", "UART", "RS232", "GPIB"]
        }
    }
}

REPO_MAP = {
    # --- SYS 12: LANGUAGES ---
    "COBOL": "https://github.com/gnucobol/gnucobol.git",
    "FORTRAN": "https://github.com/gcc-mirror/gcc.git",
    "ALGOL": "https://github.com/maroon/algol68g.git",
    "LISP": "https://github.com/sbcl/sbcl.git",
    "BASIC": "https://github.com/chip-basic/chip-basic.git",
    "PASCAL": "https://github.com/fpc/FPCSource.git",
    "ADA": "https://github.com/gcc-mirror/gcc.git",
    "SIMULA": "https://github.com/portablesimula/github.io.git",
    
    "ASM_x86": "https://github.com/nasm/nasm.git",
    "ASM_ARM": "https://github.com/ARM-software/arm-trusted-firmware.git",
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
    
    "SQL": "https://github.com/postgres/postgres.git",
    "R": "https://github.com/wch/r-source.git",
    "JULIA": "https://github.com/JuliaLang/julia.git",
    
    "BRAINFUCK": "https://github.com/brain-lang/brainfuck.git",
    "WHITESPACE": "https://github.com/hostilefork/whitespacers.git",

    # --- SYS 13: PLATFORMS ---
    "DOS": "https://github.com/microsoft/MS-DOS.git",
    "WINDOWS_3X": "https://github.com/microsoft/winfile.git",
    "WINDOWS_NT": "https://github.com/reactos/reactos.git",
    "OS_2": "https://github.com/psmedley/os2-ports.git",
    "AMIGA_OS": "https://github.com/aros-development-team/aros.git",
    
    "SOLARIS": "https://github.com/illumos/illumos-gate.git",
    "FREEBSD": "https://github.com/freebsd/freebsd-src.git",
    "OPENBSD": "https://github.com/openbsd/src.git",
    "NETBSD": "https://github.com/NetBSD/src.git",
    "DRAGONFLY": "https://github.com/DragonFlyBSD/DragonFlyBSD.git",
    
    "VXWORKS": "https://github.com/Wind-River/vxworks7-layer-for-qemu.git",
    "QNX": "https://github.com/qnx/qnx-reference.git",
    "FREERTOS": "https://github.com/FreeRTOS/FreeRTOS-Kernel.git",
    "ZEPHYR": "https://github.com/zephyrproject-rtos/zephyr.git",
    
    "ANDROID": "https://github.com/aosp-mirror/platform_manifest.git",
    
    # --- SYS 14: ACCESSORS ---
    "TN3270": "https://github.com/rbbrittain/x3270.git",
    "TN5250": "https://github.com/tn5250/tn5250.git",
    "X11": "https://github.com/freedesktop/xorg-server.git",
    "WAYLAND": "https://github.com/wayland-project/wayland.git",
    
    "TCP_IP": "https://github.com/lwip-tcpip/lwip.git",
    "DNS": "https://github.com/isc-projects/bind9.git",
    "DHCP": "https://github.com/isc-projects/kea.git",
    
    "GRPC": "https://github.com/grpc/grpc.git",
    "THRIFT": "https://github.com/apache/thrift.git",
    "KAFKA": "https://github.com/apache/kafka.git",
    "MQTT": "https://github.com/eclipse/mosquitto.git",
    "REST": "https://github.com/OAI/OpenAPI-Specification.git",
    "GRAPHQL": "https://github.com/graphql/graphql-js.git"
}

def audit():
    total_count = 0
    missing_count = 0
    
    print("\n=== SOVEREIGNTY COVERAGE AUDIT ===\n")
    
    for sys_name, sys_data in SYSTEMS.items():
        print(f"[{sys_name}]")
        for cat_name, items in sys_data["categories"].items():
            missing_in_cat = []
            for item in items:
                total_count += 1
                if item not in REPO_MAP:
                    missing_in_cat.append(item)
                    missing_count += 1
            
            if missing_in_cat:
                print(f"  {cat_name}:")
                for m in missing_in_cat:
                    print(f"    - {m}")
    
    print("\n-----------------------------------")
    print(f"TOTAL SYSTEMS: {total_count}")
    print(f"MISSING REPOS: {missing_count}")
    print(f"COVERAGE:      {((total_count - missing_count)/total_count)*100:.1f}%")
    print("-----------------------------------\n")

if __name__ == "__main__":
    audit()
