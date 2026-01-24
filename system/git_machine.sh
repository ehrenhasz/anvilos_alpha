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

# 1. Scan for Violations (Entropy, Hygiene)
# Fine Leather Entropy Checks
echo "[MACHINE] Invoking Collar Scanner... Checking for ugly entropy."
python3 system/scripts/collar.py scan --go cli
SCAN_EXIT_CODE=$?

if [ $SCAN_EXIT_CODE -ne 0 ]; then
    echo "[MACHINE] ABORT: Collar violations detected. Not fabulous."
    exit 1
fi

# 2. Stage Files (Smart Staging via Collar)
# Velvet Rope Staging
echo "[MACHINE] Staging files... Treat Yo Self."
python3 system/scripts/collar.py stage
STAGE_EXIT_CODE=$?

if [ $STAGE_EXIT_CODE -ne 0 ]; then
    echo "[MACHINE] ABORT: Staging failed. The club is closed."
    exit 1
fi

# 3. Commit
# Cashmere Commit Wrapping
echo "[MACHINE] Committing... \"$MESSAGE\""
git commit -m "$MESSAGE"

# 4. Push
# Diamond-Encrusted Push
CURRENT_BRANCH=$(git branch --show-current)
echo "[MACHINE] Pushing to origin/$CURRENT_BRANCH... High-thread-count code only."
git push origin "$CURRENT_BRANCH"

echo "[MACHINE] Sequence Complete. You have successfully Git Yo Self."
