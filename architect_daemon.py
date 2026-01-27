import os
import sys
import json
import uuid
import time
import logging

# --- PATH RESOLUTION ---

PROJECT_ROOT = os.path.dirname(os.path.abspath(__file__))
SYSTEM_DB = os.path.join(PROJECT_ROOT, "data", "cortex.db")
TOKEN_PATH = os.path.join(PROJECT_ROOT, "config", "token")
sys.path.append(os.path.join(PROJECT_ROOT, "src"))
sys.path.append(os.path.join(PROJECT_ROOT, "vendor"))

from google import genai
from google.genai import types

from anvilos.cortex.db_interface import CortexDB

# --- LOGGING ---
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - [ARCHITECT] - %(levelname)s - %(message)s'
)
logger = logging.getLogger()

# --- AUTHENTICATION ---
API_KEY = None
if os.path.exists(TOKEN_PATH):
    with open(TOKEN_PATH, 'r') as f:
        API_KEY = f.read().strip()

if not API_KEY:
    logger.error("API Key not found. Architect cannot reason.")
    sys.exit(1)

CLIENT = genai.Client(api_key=API_KEY)
DB = CortexDB(SYSTEM_DB)

def architect_recipe(goal):
    """
    Uses Gemini to break down a goal into a series of MicroJSON punch cards.
    """
    logger.info(f"Architecting recipe for goal: {goal}")
    
    prompt = f"""
    You are the ARCHITECT of AnvilOS. Your job is to take a high-level goal and break it down into a sequence of atomic 'Punch Cards' for the PROCESSOR.
    
    The schema for a card is:
    {{
        "op": "OPERATION_CODE",
        "pld": {{ ... payload ... }}
    }}
    
    Supported OPs:
    - SYS_CMD: {{ "cmd": "bash command", "timeout": int }}
    - GEN_OP: {{ "type": "code/config/test", "prompt": "instruction" }}
    - FILE_WRITE: {{ "path": "path/to/file", "content": "text" }}
    
    GUIDELINES:
    - Use 'sudo' for commands that require root privileges.
    - Use '/mnt/anvil_temp' for temporary storage. DO NOT use '/mnt' directly.
    - Use absolute paths.
    - Use '-s' for 'parted'.
    - GENTOO COMMANDS: 
        - Use fully-qualified package names (e.g., 'dev-vcs/git' instead of 'git').
        - NEVER use '--ask'. Use '--verbose' or '--quiet' instead.
        - Use 'emerge --sync' before installing new packages if needed.
    - RESOURCE LIMITS:
        - ALWAYS limit 'make' and 'cargo build' concurrency to '-j2'.
        - If configuring a VM, restrict RAM to 50% of host (e.g. 512MB).
    - IMPORTANT: Each card runs in a SEPARATE shell. Shell variables (like STAGE3_URL) will NOT persist between cards.
    - To preserve state between steps, write to temporary files or bundle multiple related commands into a single 'SYS_CMD' using '&&' or a multi-line script.
    - For Gentoo Stage3, it is better to have one big SYS_CMD that downloads, verifies, and extracts in one go to keep variables local.
    - When creating a disk image, use 'losetup -P' to handle partitions.
    - Use 'mkfs.xfs -f' to force formatting.
    - Use 'losetup -f' to find a free loop device if needed, or assume the Processor will remap /dev/loop0.
    
    Goal: {goal}
    
    Return ONLY a JSON list of cards. No preamble, no markdown blocks.
    """
    
    try:
        response = CLIENT.models.generate_content(
            model="gemini-2.0-flash",
            contents=prompt,
            config=types.GenerateContentConfig(
                temperature=0.1,
                response_mime_type="application/json"
            )
        )
        
        recipe = json.loads(response.text)
        return recipe
    except Exception as e:
        logger.error(f"Reasoning Error: {e}")
        return None

def main_loop():
    logger.info("Architect Daemon Online. Monitoring sys_goals...")
    
    while True:
        try:
            goal_row = DB.fetch_pending_goal()
            
            if goal_row:
                goal_id = goal_row['id']
                goal_text = goal_row['goal']
                
                logger.info(f"Processing Goal {goal_id}: {goal_text}")
                
                # Mark goal as processing (stat=1)
                DB.update_goal_status(goal_id, 1)
                
                recipe = architect_recipe(goal_text)
                
                if recipe:
                    logger.info(f"Recipe generated with {len(recipe)} cards.")
                    # Insert cards into card_stack using CortexDB
                    for i, card in enumerate(recipe):
                        card_id = str(uuid.uuid4())
                        DB.push_card(card_id, i, card['op'], card['pld'])
                    
                    # Mark goal as completed (stat=2)
                    DB.update_goal_status(goal_id, 2)
                    logger.info(f"Goal {goal_id} committed to Card Stack.")
                else:
                    # Mark goal as failed (stat=9)
                    DB.update_goal_status(goal_id, 9)
                    logger.error(f"Failed to architect recipe for goal {goal_id}")
                
        except Exception as e:
            logger.error(f"Loop Error: {e}")
            
        time.sleep(5)

if __name__ == "__main__":
    main_loop()