import json
import re

with open("installed_binaries.json", "r") as f:
    data = json.load(f)

binaries = data["binaries"]

# Sovereignty Protocol / Anvil Doctrine Filters
FORBIDDEN_PATTERNS = {
    "COMPILERS_BUILD": [
        r"^gcc.*", r"^g\+\+.*", r"^make$", r"^cmake$", r"^clang.*", 
        r"^automake", r"^autoconf", r"^as$", r"^ld$", r"^gdb$"
    ],
    "INTERPRETERS": [
        r"^python[0-9.]*$", r"^perl.*", r"^ruby.*", r"^php.*", r"^lua.*", r"^node.*", r"^npm.*"
    ],
    "PACKAGE_MGMT": [
        r"^apt.*", r"^dpkg.*", r"^snap.*", r"^flatpak.*", r"^rpm.*", r"^alien.*"
    ],
    "NETWORK_CLIENTS": [
        r"^curl$", r"^wget$", r"^ftp$", r"^telnet$", r"^netcat$", r"^nc$", 
        r"^ssh$", r"^scp$", r"^rsync$", r"^sftp$"
    ],
    "EDITORS_DOCS": [
        r"^vim.*", r"^nano$", r"^emacs.*", r"^man$", r"^info$", r"^less$", r"^more$"
    ]
}

candidates = {k: [] for k in FORBIDDEN_PATTERNS.keys()}
total_size = 0

for b in binaries:
    name = b["name"]
    path = b["path"]
    size = b["size"]
    
    # Skip Anvil/System Criticals if they happen to match (unlikely for these strict patterns but good practice)
    if name == "python3" and "/usr/bin/python3" in path:
        # Keep system python for now as 'bigiron.py' and system scripts likely depend on it
        # until "Lobotomy" phase.
        continue

    for category, patterns in FORBIDDEN_PATTERNS.items():
        for pat in patterns:
            if re.match(pat, name):
                candidates[category].append(path)
                total_size += size
                break

print(f"ANALYSIS REPORT (Based on Anvil Doctrine)")
print(f"----------------------------------------")
for cat, items in candidates.items():
    print(f"{cat}: {len(items)} binaries")
    if len(items) > 0:
        print(f"  Examples: {', '.join([x.split('/')[-1] for x in items[:5]])}...")

print(f"----------------------------------------")
print(f"Total Candidate Candidates for Removal: {sum(len(l) for l in candidates.values())}")
print(f"Total Size on Disk: {total_size / 1024 / 1024:.2f} MB")

with open("exclusion_candidates.json", "w") as f:
    json.dump(candidates, f, indent=2)
