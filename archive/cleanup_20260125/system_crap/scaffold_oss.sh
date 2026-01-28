BASE_DIR="oss_sovereignty"
mkdir -p "$BASE_DIR"
while IFS= read -r line; do
  if [[ "$line" =~ ^-\ (.*) ]]; then
    component_name="${BASH_REMATCH[1]}"
    component_dir="$BASE_DIR/${component_name}"
    mkdir -p "$component_dir"
    echo "Created directory: $component_dir"
  fi
done < DOCS/.oss_list.md
echo "Scaffolding complete."
