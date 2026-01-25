import json
import datetime

QUEUE_FILE = "runtime/card_queue.json"

new_cards = [
    {
        "id": "sys_09_mpy_fetch",
        "description": "ANVIL_COMPILER: Fetch MicroPython source (v1.23+). Configure for static Musl build (x86_64-unknown-linux-musl). This is the runtime engine for the Anvil.",
        "status": "pending",
        "created_at": datetime.datetime.now().isoformat(),
        "result": None
    },
    {
        "id": "dev_anvil_src",
        "description": "ANVIL_COMPILER: Implement 'system/anvil/anvil.py'. This is the sovereign compiler logic. It parses 'forge.mjson' and orchestrates the build using the static toolchain and mpy-cross.",
        "status": "pending",
        "created_at": datetime.datetime.now().isoformat(),
        "result": None
    },
    {
        "id": "ops_anvil_bootstrap",
        "description": "ANVIL_COMPILER: Bootstrap 'bin/anvil'. Compile 'anvil.py' logic into frozen bytecode and link with the static MicroPython runtime to produce the final hermetic binary.",
        "status": "pending",
        "created_at": datetime.datetime.now().isoformat(),
        "result": None
    }
]

try:
    with open(QUEUE_FILE, "r") as f:
        queue = json.load(f)
    
    # Prepend new P1 cards
    queue = new_cards + queue
    
    with open(QUEUE_FILE, "w") as f:
        json.dump(queue, f, indent=2)
    
    print(f"Successfully injected {len(new_cards)} P1 cards.")

except Exception as e:
    print(f"Error: {e}")
