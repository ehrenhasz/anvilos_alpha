#!/bin/bash
# scripts/launch_hud.sh
# Sets up the "Meat-Space" HUD: Cortex (Left), BigIron (TopRight), Btop (BotRight)

# Get the directory of the script itself, then go up one level to the project root
REPO_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"

# 1. Launch Left Window: Cortex (Gemini)
# (Labwc window rules will handle positioning)
xfce4-terminal --title="GEMINI_CORTEX" --geometry=100x40 --command="bash -c 'cd $REPO_DIR && node cortex.js; exec bash'" &
sleep 1

# 2. Launch Top Right: Big Iron
xfce4-terminal --title="BIG_IRON" --command="bash -c 'python3 $REPO_DIR/system/dashboard.py'" &
sleep 1

# 3. Launch Bottom Right: Btop
xfce4-terminal --title="SYSTEM_MONITOR" --command="btop" &

echo ">> HUD INITIALIZED."
