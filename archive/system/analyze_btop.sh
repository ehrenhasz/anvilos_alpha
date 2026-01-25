#!/bin/bash

# Analyze anvil.build.sh to find the btop source URL and clone command
BTOP_DIR="oss_sovereignty/sys_07_Btop"

# Check if the directory exists
if [ ! -d "$BTOP_DIR" ]; then
  echo ">> [ANVIL] '$BTOP_DIR' does not exist. Analyzing anvil.build.sh for clone instructions..."

  # Extract relevant lines from anvil.build.sh (adjust based on actual content)
  CLONE_CMD=

  if [ -z "$CLONE_CMD" ]; then
    echo ">> [ANVIL] Could not find git clone command in anvil.build.sh. Looking for wget command."
    CLONE_CMD=
    if [ -z "$CLONE_CMD" ]; then
      echo ">> [ANVIL] Could not find wget command in anvil.build.sh. Unable to determine download method."
      exit 1
    fi
    echo ">> [ANVIL] Found wget command: $CLONE_CMD"
    eval "$CLONE_CMD"
    mv btop "$BTOP_DIR" #Adjust this command.
  else
    echo ">> [ANVIL] Found git clone command: $CLONE_CMD"
    eval "$CLONE_CMD"
  fi

  # After cloning, rename the directory if necessary (adjust based on actual content of anvil.build.sh)
  # For example, if it clones into a directory named 'btop-master':
  # mv btop-master "$BTOP_DIR"

else
  echo ">> [ANVIL] '$BTOP_DIR' already exists. Skipping clone."
fi

# Disconnect the git repository
if [ -d "$BTOP_DIR/.git" ]; then
  echo ">> [ANVIL] '$BTOP_DIR' is a git repository. Removing remote 'origin'."
  cd "$BTOP_DIR"
  git remote remove origin
  cd ../..
  echo ">> [ANVIL] Removed remote 'origin'."
else
  echo ">> [ANVIL] '$BTOP_DIR' is not a git repository."
fi

# Verify that remote 'origin' is removed
if [ -d "$BTOP_DIR" ]; then
    if [ -d "$BTOP_DIR/.git" ]; then
        cd "$BTOP_DIR"
        REMOTE_ORIGIN=
        cd ../..
        if [ -z "$REMOTE_ORIGIN" ]; then
            echo ">> [ANVIL] SUCCESS: Remote 'origin' does not exist in '$BTOP_DIR'."
        else
            echo ">> [ANVIL] ERROR: Remote 'origin' still exists in '$BTOP_DIR': $REMOTE_ORIGIN"
            exit 1
        fi
    else
        echo ">> [ANVIL] SUCCESS: Directory '$BTOP_DIR' exists, but is not a git repo."
    fi
else
    echo ">> [ANVIL] ERROR: Directory '$BTOP_DIR' does not exist."
    exit 1
fi

exit 0
