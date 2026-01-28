#!/bin/bash
# system/run_sovereignty_task.sh
# General purpose sovereignty task runner.

TARGET_DIR="${1:-oss_sovereignty/ctx_06_recharts}"
FOLDER_NAME=$(basename "$TARGET_DIR")
BUILD_SCRIPT="${2:-$TARGET_DIR/anvil.build.sh}"

echo ">> TARGET_DIR: $TARGET_DIR"
echo ">> BUILD_SCRIPT: $BUILD_SCRIPT"

# 1. ANALYZE: Check build script to identify the source repository URL.
if [ -f "$BUILD_SCRIPT" ]; then
    # Try to extract wget URL
    DOWNLOAD_URL="$(grep "wget" "$BUILD_SCRIPT" | head -n 1 | awk '{print $2}' | tr -d '"')"
    TARBALL_NAME="$(basename "$DOWNLOAD_URL")"
else
    echo "Warning: Build script '$BUILD_SCRIPT' not found. Skipping URL analysis."
fi

# Check if the target directory exists
if [ ! -d "$TARGET_DIR" ]; then
    echo "Target directory '$TARGET_DIR' does not exist. Creating it."
    mkdir -p "$TARGET_DIR"
fi

# 2. CLONE/DOWNLOAD: If a URL was found and source code is not present, download it.
if [ -n "$DOWNLOAD_URL" ] && [ "$DOWNLOAD_URL" != "." ]; then
    if [ ! -f "$TARGET_DIR/$TARBALL_NAME" ]; then
        echo "Source code not found. Downloading from: $DOWNLOAD_URL"
        cd "$TARGET_DIR" || exit 1
        wget "$DOWNLOAD_URL"
        cd - > /dev/null
    else
        echo "Source code found in '$TARGET_DIR'."
    fi
fi

# 3. DISCONNECT: Find and disconnect the git repository (if it exists).
if [ -d "$TARGET_DIR/.git" ] || [ -d "$TARGET_DIR/source/.git" ]; then
  GIT_PATH="$TARGET_DIR"
  [ -d "$TARGET_DIR/source/.git" ] && GIT_PATH="$TARGET_DIR/source"
  
  echo "Found .git directory in $GIT_PATH. Disconnecting..."
  cd "$GIT_PATH" || exit 1
  if git remote | grep -q "origin"; then
      git remote remove origin
      echo "Disconnected git repository in '$GIT_PATH'."
  else
      echo "No remote 'origin' found in '$GIT_PATH'."
  fi
  cd - > /dev/null
else
  echo "No .git directory found in '$TARGET_DIR' or '$TARGET_DIR/source'."
fi

# 4. REPORT: Output the status of the repository.
if [ -d "$TARGET_DIR/.git" ] || [ -d "$TARGET_DIR/source/.git" ]; then
    GIT_PATH="$TARGET_DIR"
    [ -d "$TARGET_DIR/source/.git" ] && GIT_PATH="$TARGET_DIR/source"
    cd "$GIT_PATH" || exit 1
    if git remote | grep -q "origin"; then
        echo "ERROR: Remote 'origin' still exists in $GIT_PATH."
    else
        echo "SUCCESS: No remote 'origin' found in $GIT_PATH."
    fi
    cd - > /dev/null
else
    echo "SUCCESS: No git repository to report on."
fi
