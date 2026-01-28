import json
import os
import shutil

# Load the exclusion candidates (the 158 flagged binaries)
try:
    with open("exclusion_candidates.json", "r") as f:
        data = json.load(f)
except FileNotFoundError:
    print("exclusion_candidates.json not found.")
    exit(1)

# Base directory for the folders
BASE_DIR = "oss_sovereignty/legacy_bin"
os.makedirs(BASE_DIR, exist_ok=True)

# Iterate through categories and create folders/cards
count = 0
for category, paths in data.items():
    # Create category folder
    cat_dir = os.path.join(BASE_DIR, category)
    os.makedirs(cat_dir, exist_ok=True)
    
    for path in paths:
        bin_name = os.path.basename(path)
        # Create a folder for the binary
        bin_dir = os.path.join(cat_dir, bin_name)
        os.makedirs(bin_dir, exist_ok=True)
        
        # Create a "card" (metadata file) for it
        card_content = {
            "id": f"legacy_{bin_name}",
            "original_path": path,
            "category": category,
            "status": "flagged_for_removal",
            "replacement_strategy": "rewrite_in_anvil"
        }
        
        with open(os.path.join(bin_dir, "card.json"), "w") as f:
            json.dump(card_content, f, indent=2)
            
        count += 1

print(f"Created {count} folders and cards in {BASE_DIR}")
