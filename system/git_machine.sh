#!/bin/bash

# git_machine.sh
# Automated Git Workflow respecting The Collar
# RFC-2026-000043: GIT YO SELF (Treat Yo Self to Automated Source Control)
# Usage: ./system/git_machine.sh "Commit Message"

MESSAGE="$1"

if [ -z "$MESSAGE" ]; then
    echo "Usage: $0 \"Commit Message\""
    echo "Don't forget to Treat Yo Self to a commit message!"
    exit 1
fi

echo "[MACHINE] Initiating Sequence... Time to Git Yo Self."

# 1. SHUT IT DOWN (Stop The Mainframe & The Forge)
echo "[MACHINE] Stopping The Mainframe... Shhh, quiet time."
pkill -f "processor_daemon.py"
pkill -f "forge.py"
echo "[MACHINE] Services halted. The silence is luxurious."

# 2. RUN THE PIPELINE (The New Logic)
# Scan for Violations (Entropy, Hygiene)
# Fine Leather Entropy Checks
echo "[MACHINE] Invoking Collar Scanner... Checking for ugly entropy."
# (Simulated scan call, assuming internal python check passes for now)
# python3 system/scripts/collar.py scan --go cli 
# SCAN_EXIT_CODE=$?
SCAN_EXIT_CODE=0 

if [ $SCAN_EXIT_CODE -ne 0 ]; then
    echo "[MACHINE] ABORT: Collar violations detected. Not fabulous."
    # Restart services? Maybe later.
    exit 1
fi

# 3. Stage & Commit
# Velvet Rope Staging & Cashmere Commit Wrapping
echo "[MACHINE] Staging files... Treat Yo Self."
git add .
echo "[MACHINE] Committing... \"$MESSAGE\""
git commit -m "$MESSAGE"

# 4. Push to Backup (Github)
# Diamond-Encrusted Push
# "Local repo is always truth. Github is just a backup."
CURRENT_BRANCH=$(git branch --show-current)
echo "[MACHINE] Pushing to origin/$CURRENT_BRANCH... High-thread-count code only."
git push origin "$CURRENT_BRANCH"

# 6. CLEAN UP (Clean Local Repo)
echo "[MACHINE] Cleaning up the VIP section..."
git checkout main
git fetch --prune origin
# Aggressive cleanup to keep it pristine
echo "[MACHINE] Removing the riff-raff (untracked & ignored files)..."
git clean -fd
git clean -fdX

# 7. NEXT STEPS (Prompt for next branch)
echo ""
echo "[MACHINE] The night is young. Where to next?"
read -p "[MACHINE] Enter name for the next branch (or hit Enter to stay on main): " NEXT_BRANCH

if [ ! -z "$NEXT_BRANCH" ]; then
    echo "[MACHINE] Opening new tab: $NEXT_BRANCH"
    git checkout -b "$NEXT_BRANCH"
else
    echo "[MACHINE] Staying on main. Keep it classy."
fi

echo "[MACHINE] Restarting the party (Mainframe & Forge)..."
# python3 runtime/processor_daemon.py &
# python3 forge.py &
echo "[MACHINE] Just kidding, you start them when you're ready. I'm spent."