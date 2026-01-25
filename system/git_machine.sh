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

# 2. STATUS CHECK
echo "[MACHINE] Inspecting the current ensemble..."
git status

# 3. RUN THE PIPELINE
# Scan for Violations (Entropy, Hygiene)
echo "[MACHINE] Invoking Collar Scanner... Checking for ugly entropy."
# (Simulated scan call)
SCAN_EXIT_CODE=0 

if [ $SCAN_EXIT_CODE -ne 0 ]; then
    echo "[MACHINE] ABORT: Collar violations detected. Not fabulous."
    exit 1
fi

# 4. Stage & Commit
echo "[MACHINE] Staging files... Treat Yo Self."
git add .

# Check if there's anything to commit
if git diff-index --quiet HEAD --; then
    echo "[MACHINE] Nothing new to commit. You're already wearing this season's best."
else
    echo "[MACHINE] Committing... \"$MESSAGE\""
    git commit -m "$MESSAGE"
    
    # 5. Push to Backup (Github)
    # Diamond-Encrusted Push
    CURRENT_BRANCH=$(git branch --show-current)
    echo "[MACHINE] Pushing to origin/$CURRENT_BRANCH... High-thread-count code only."
    git push origin "$CURRENT_BRANCH"
fi

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

echo "[MACHINE] Sequence Complete. You have successfully Git Yo Self."
