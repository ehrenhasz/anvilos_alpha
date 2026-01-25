#!/bin/bash

# git_machine.sh
# Automated Git Workflow respecting The Collar
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
pkill -f "forge.py" || true
echo "[MACHINE] Services halted."

# 2. CAPTURE STATE
ORIGINAL_BRANCH=$(git branch --show-current)
echo "[MACHINE] Working on branch: $ORIGINAL_BRANCH"

# 3. STAGE & COMMIT
echo "[MACHINE] Staging files..."
git add .

if git diff-index --quiet HEAD --; then
    echo "[MACHINE] Nothing new to commit."
else
    echo "[MACHINE] Committing... \"$MESSAGE\""
    git commit -m "$MESSAGE"
fi

# 4. PUSH FEATURE
echo "[MACHINE] Pushing $ORIGINAL_BRANCH to origin..."
git push origin "$ORIGINAL_BRANCH"

# 5. MERGE PHASE
if [ "$ORIGINAL_BRANCH" != "main" ]; then
    echo "[MACHINE] Switching to main for integration..."
    git checkout main
    echo "[MACHINE] Pulling latest main..."
    git pull origin main
    
    echo "[MACHINE] Merging feature branch: $ORIGINAL_BRANCH"
    if git merge "$ORIGINAL_BRANCH"; then
        echo "[MACHINE] Merge successful."
    else
        echo "[MACHINE] CONFLICT DETECTED during local merge. Aborting auto-merge."
        exit 1
    fi
fi

# 6. MERGE OTHER OPEN BRANCHES
# Detect other remote branches that are not main or HEAD
echo "[MACHINE] Scanning for other open remote branches..."
REMOTE_BRANCHES=$(git branch -r | grep -v '\->' | grep -v "origin/main" | grep -v "origin/HEAD" | sed 's/origin\///')

for branch in $REMOTE_BRANCHES; do
    # Skip if it's the branch we just merged (though logic above handles local, this handles remote ref)
    if [ "$branch" == "$ORIGINAL_BRANCH" ]; then
        continue
    fi
    
    echo "[MACHINE] Found open branch: $branch. Attempting merge..."
    if git merge "origin/$branch"; then
        echo "[MACHINE] Successfully merged $branch."
    else
        echo "[MACHINE] WARN: Could not auto-merge $branch due to conflicts. Skipping."
        git merge --abort
    fi
done

# 7. PUSH MAIN
echo "[MACHINE] Pushing integrated main to origin..."
git push origin main

# 8. CLEAN UP
echo "[MACHINE] Cleaning up workspace..."
git clean -fd
git clean -fdX

# 9. NEXT STEPS
echo ""
echo "[MACHINE] Sequence Complete."
read -p "[MACHINE] Enter name for the next branch (or hit Enter to stay on main): " NEXT_BRANCH

if [ ! -z "$NEXT_BRANCH" ]; then
    echo "[MACHINE] Creating new branch: $NEXT_BRANCH"
    git checkout -b "$NEXT_BRANCH"
else
    echo "[MACHINE] Staying on main."
fi