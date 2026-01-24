#!/bin/bash

# Script to scaffold the oss_sovereignty directory structure based on .oss_list.md

# Define the base directory
BASE_DIR="oss_sovereignty"

# Ensure the base directory exists
mkdir -p "$BASE_DIR"

# Read the .oss_list.md file and extract component names
while IFS= read -r line; do
  # Extract component names from lines starting with "- "
  if [[ "$line" =~ ^-\ (.*) ]]; then
    component_name="${BASH_REMATCH[1]}"
    # Create a directory for each component
    component_dir="$BASE_DIR/${component_name}"
    mkdir -p "$component_dir"
    echo "Created directory: $component_dir"
  fi
done < DOCS/.oss_list.md

echo "Scaffolding complete."
