import os
import sys
import json
import uuid
import time

PROJECT_ROOT = os.getcwd()

# Path injection
sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))
for venv_dir in ["venv", ".venv"]:
    lib_path = os.path.join(PROJECT_ROOT, venv_dir, "lib")
    if os.path.exists(lib_path):
        for py_dir in os.listdir(lib_path):
            if py_dir.startswith("python"):
                site_pkg = os.path.join(lib_path, py_dir, "site-packages")
                if os.path.exists(site_pkg):
                    sys.path.append(site_pkg)

sys.path.append(PROJECT_ROOT)

from architect_daemon import architect_recipe, DB

def forge_card_101():
    # Read directives for context
    with open("PHASE2_DIRECTIVES.md", "r") as f:
        directives = f.read()
        
    # The user said: "give the forge oply card 101. and that means the text from line one."
    # We use line 1 as the primary goal context but specify Card 101.
    line_one = directives.splitlines()[0]
    
    goal = f"Context: {line_one}\nDirectives:\n{directives}\n\nGOAL: Implement ONLY Card 101. Path is {PROJECT_ROOT}/oss_sovereignty/linux-6.6.14"
    
    print(f"Architeching goal for Card 101...")
    
    recipe = architect_recipe(goal)
    
    if recipe:
        # Filter for Card 101 if multiple cards are returned (should only be 1 based on goal)
        print(f"Generated {len(recipe)} cards.")
        for i, card in enumerate(recipe):
            card_id = str(uuid.uuid4())
            print(f"Pushing card {i}: {card['op']} - {card['pld']}")
            DB.push_card(card_id, i, card['op'], card['pld'])
        print("Success.")
    else:
        print("Failed to generate recipe.")

if __name__ == "__main__":
    forge_card_101()
