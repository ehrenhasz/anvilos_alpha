import json
import os
import math

# Known mappings for critical binaries
REPO_MAP = {
    "make": "https://git.savannah.gnu.org/git/make.git",
    "wget": "https://git.savannah.gnu.org/git/wget.git",
    "rsync": "https://github.com/WayneD/rsync.git",
    "vim": "https://github.com/vim/vim.git",
    "vim.basic": "https://github.com/vim/vim.git",
    "vim.tiny": "https://github.com/vim/vim.git",
    "nano": "https://git.savannah.gnu.org/git/nano.git",
    "less": "https://github.com/gwsw/less.git",
    "screen": "https://git.savannah.gnu.org/git/screen.git",
    "perl": "https://github.com/Perl/perl5.git",
    "python3": "https://github.com/python/cpython.git",
    "node": "https://github.com/nodejs/node.git",
    "dpkg": "https://git.dpkg.org/git/dpkg/dpkg.git",
    "apt": "https://salsa.debian.org/apt-team/apt.git",
    "snap": "https://github.com/snapcore/snapd.git",
    "ssh": "https://github.com/openssh/openssh-portable.git",
    "scp": "https://github.com/openssh/openssh-portable.git",
    "sftp": "https://github.com/openssh/openssh-portable.git",
    "man": "https://git.savannah.nongnu.org/git/man-db.git",
    "more": "https://github.com/util-linux/util-linux.git"
}

LEGACY_ROOT = "oss_sovereignty/legacy_bin"
QUEUE_FILE = "runtime/card_queue.json"

def get_legacy_items():
    items = []
    if not os.path.exists(LEGACY_ROOT):
        return items
        
    for cat in os.listdir(LEGACY_ROOT):
        cat_path = os.path.join(LEGACY_ROOT, cat)
        if not os.path.isdir(cat_path):
            continue
            
        for binary in os.listdir(cat_path):
            if binary == "curl": continue # Already done
            
            # Try to match binary name to repo map (fuzzy or exact)
            url = REPO_MAP.get(binary)
            
            # Handle versions like perl5.38.2 -> perl
            if not url:
                for k, v in REPO_MAP.items():
                    if binary.startswith(k):
                        url = v
                        break
            
            items.append({
                "name": binary,
                "category": cat,
                "path": os.path.join(cat_path, binary),
                "url": url
            })
    return items

def mint_cards():
    items = get_legacy_items()
    # Sort by name for stability
    items.sort(key=lambda x: x["name"])
    
    # Load existing queue
    queue = []
    if os.path.exists(QUEUE_FILE):
        with open(QUEUE_FILE, "r") as f:
            queue = json.load(f)

    # Chunk into groups of 5
    BATCH_SIZE = 5
    new_cards = []
    
    for i in range(0, len(items), BATCH_SIZE):
        batch = items[i:i+BATCH_SIZE]
        
        # Construct Description and Command
        names = [b["name"] for b in batch]
        
        # Command Construction:
        # We need a way to run multiple assimilations in one card.
        # We'll create a temporary shell script or chain python commands.
        # Chaining python commands is safer.
        
        cmds = []
        for b in batch:
            if b["url"]:
                cmds.append(f"python3 system/scripts/assimilate.py {b['path']} {b['url']}")
            else:
                cmds.append(f"echo 'SKIPPING {b['name']} - NO URL FOUND'")
        
        full_command = " && ".join(cmds)
        
        card = {
            "id": f"sov_batch_{i//BATCH_SIZE + 1:03d}_{names[0]}",
            "description": f"SOVEREIGNTY BATCH {i//BATCH_SIZE + 1}: Assimilate {', '.join(names)}",
            "status": "pending",
            "command": full_command,
            "created_at": "2026-01-19T12:00:00.000Z",
            "batch_details": batch
        }
        new_cards.append(card)

    # Append new cards
    queue.extend(new_cards)
    
    with open(QUEUE_FILE, "w") as f:
        json.dump(queue, f, indent=2)
        
    print(f"Minted {len(new_cards)} cards covering {len(items)} binaries.")

if __name__ == "__main__":
    mint_cards()
