import os
import json
from datetime import datetime

ROOT = "oss_sovereignty"

SYSTEMS = {
    "sys_12_Languages": {
        "description": "THE TONGUE: Sovereign implementations of all major coding languages.",
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
        "description": "THE FOUNDATION: Re-implementations of world operating systems and mainframes.",
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
        "description": "THE GATE: Protocols, Terminals, and Interfaces.",
        "categories": {
            "01_Terminals": ["TN3270", "TN5250", "VT100", "VT220", "ANSI", "X11", "WAYLAND"],
            "02_Network_Stack": ["TCP_IP", "UDP", "SCTP", "ICMP", "IGMP", "ARP", "DNS", "DHCP"],
            "03_Legacy_Net": ["SNA", "DECNET", "IPX_SPX", "APPLETALK", "X_25", "FRAME_RELAY"],
            "04_RPC_Messaging": ["GRPC", "THRIFT", "CORBA", "SOAP", "REST", "GRAPHQL", "MQTT", "AMQP", "KAFKA"],
            "05_Hardware_Bus": ["USB", "PCIE", "I2C", "SPI", "CAN", "UART", "RS232", "GPIB"]
        }
    }
}

def create_structure():
    print(f">> SCAFFOLDING THE WORLD IN {ROOT}...")
    
    for sys_name, sys_data in SYSTEMS.items():
        sys_path = os.path.join(ROOT, sys_name)
        
        # Create System Root
        if not os.path.exists(sys_path):
            os.makedirs(sys_path)
            
        # System Metadata
        meta = {
            "description": sys_data["description"],
            "created_at": datetime.now().isoformat(),
            "status": "placeholder"
        }
        with open(os.path.join(sys_path, "metadata.json"), "w") as f:
            json.dump(meta, f, indent=2)
            
        # Create Categories and Items
        for cat_name, items in sys_data["categories"].items():
            cat_path = os.path.join(sys_path, cat_name)
            if not os.path.exists(cat_path):
                os.makedirs(cat_path)
                
            for item in items:
                item_path = os.path.join(cat_path, item)
                if not os.path.exists(item_path):
                    os.makedirs(item_path)
                    os.makedirs(os.path.join(item_path, "source"))
                    os.makedirs(os.path.join(item_path, "build"))
                    
                    # Item Metadata
                    item_meta = {
                        "id": f"{sys_name}.{item}",
                        "name": item,
                        "category": cat_name,
                        "mandate": "REWRITE IN ANVIL",
                        "status": "awaiting_source"
                    }
                    with open(os.path.join(item_path, "metadata.json"), "w") as f:
                        json.dump(item_meta, f, indent=2)
                        
    print(">> SCAFFOLDING COMPLETE.")

if __name__ == "__main__":
    create_structure()
