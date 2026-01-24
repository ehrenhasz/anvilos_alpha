#!/bin/bash
# ------------------------------------------------------------------
# TREAT. YO. SELF.
# It’s the best day of the year. We are not doing manual commits
# like peasants. We are doing automated, luxury syncing.
# Fine leather goods? Check.
# Mimosas? Check.
# High-quality code pushed to the cloud? Check.
#
# Here is your script, draped in the finest velvet.
# ------------------------------------------------------------------

set -e

# 1. GET THE VIBE
# Folder name is the Brand Name.
REPO_NAME=$(basename "$PWD")
GITHUB_USER="ehrenhasz"
GITHUB_EMAIL="ehren.hasz@gmail.com"

# Colors? We need them popping.
GREEN='\033[0;32m'
CYAN='\033[0;36m'
MAGENTA='\033[0;35m'
NC='\033[0m'

log() { echo -e "${MAGENTA}>> [TREAT YO SELF] $1${NC}"; }
info() { echo -e "${CYAN}   $1${NC}"; }

log "Target acquired: $GITHUB_USER/$REPO_NAME. Time to shine."

# 2. HOUSEKEEPING (NO TRASH ALLOWED)
# "Ew, David." We don't want zombie containers cramping our style.
if [ -d "rootfs" ]; then
    log "Garbage detected. Incinerating 'rootfs'. We don't do clutter."
    sudo rm -rf rootfs
fi

# 3. GIT INITIALIZATION
# New repo? Treat yo self to a fresh start.
if [ ! -d ".git" ]; then
    log "Initializing fresh repo. Smells like victory."
    git init
    git branch -M main
else
    info "Repo already initialized. Keep the momentum going."
fi

# 4. IDENTITY (DESIGNER LABELS ONLY)
# Put your name on it. Make it official.
git config user.name "$GITHUB_USER"
git config user.email "$GITHUB_EMAIL"

# 5. THE VELVET ROPE (.gitignore)
# We are the bouncers of this club.
# Build artifacts? You're not on the list.
# Node modules? Go home.
cat << EOF > .gitignore
BUILD_TMP/
ARTIFACTS/
rootfs/
*.iso
*.img
*.squashfs
*.swp
node_modules/
.env
.DS_Store
Thumbs.db
core
EOF

# 6. STAGE & COMMIT
log "Staging the goods. Fine leather goods. Mimosas. Code."
git add .

# Only commit if we have something new to show off.
if ! git diff-index --quiet HEAD --; then
    log "Locking it in. High quality commit."
    git commit -m "treat yo self: sync from local $(date '+%Y-%m-%d %H:%M:%S')"
else
    info "Nothing new to commit. You're already perfect."
fi

# 7. GITHUB SYNC
log "Talking to the cloud..."

# Check if the repo exists.
if gh repo view "$GITHUB_USER/$REPO_NAME" &>/dev/null; then
    info "Repo found. We're already famous."
    
    # Ensure remote 'origin' is linked. Connect the dots.
    if ! git remote get-url origin &>/dev/null; then
        git remote add origin "https://github.com/$GITHUB_USER/$REPO_NAME.git"
    fi
else
    log "Repo not found. Creating a new empire: '$REPO_NAME'..."
    # Create public repo. Let the world see the drip.
    gh repo create "$REPO_NAME" --public --source=. --remote=origin
fi

# 8. PUSH
log "Sending it up. First class."
git push -u origin main

log "------------------------------------------------"
log "SYNC COMPLETE. TREAT YO SELF: https://github.com/$GITHUB_USER/$REPO_NAME"
log "------------------------------------------------"
