REPO_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )/.." && pwd )"
xfce4-terminal --title="GEMINI_CORTEX" --geometry=100x40 --command="bash -c 'cd $REPO_DIR && node cortex.js; exec bash'" &
sleep 1
xfce4-terminal --title="BIG_IRON" --command="bash -c 'python3 $REPO_DIR/system/dashboard.py'" &
sleep 1
xfce4-terminal --title="SYSTEM_MONITOR" --command="btop" &
echo ">> HUD INITIALIZED."
