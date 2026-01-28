KERNEL_URL=
KERNEL_DIR="oss_sovereignty/sys_01_Linux_Kernel"
echo ">> [SOVEREIGNTY] Kernel URL: "
echo ">> [SOVEREIGNTY] Kernel Directory: "
if [ ! -d "" ]; then
  echo ">> [SOVEREIGNTY] Kernel source not found. Cloning..."
  CLONE_CMD=
  if [[ "" == "" ]]; then
    echo ">> [SOVEREIGNTY] Error: Could not find suitable clone/download command in anvil.build.sh"
    exit 1
  fi
  echo ">> [SOVEREIGNTY] Executing: "
  eval  # Execute the clone command (USE WITH CAUTION!)
  if [[ "" == *".tar.gz"* ]]; then
      tar -xzf 
      mv `tar -tf  | head -n 1 | cut -d '/' -f 1` ""
      rm 
  fi
else
  echo ">> [SOVEREIGNTY] Kernel source already present."
fi
if [ -d "/.git" ]; then
  echo ">> [SOVEREIGNTY] Found .git directory. Removing origin..."
  pushd "" > /dev/null
  git remote remove origin
  popd > /dev/null
else
  echo ">> [SOVEREIGNTY] No .git directory found."
fi
echo ">> [SOVEREIGNTY] Checking remote origin..."
if [ -d "/.git" ]; then
  pushd "" > /dev/null
  REMOTE_ORIGIN=
  popd > /dev/null
  if [ -n "" ]; then
    echo ">> [SOVEREIGNTY] ERROR: Remote origin still exists: "
  else
    echo ">> [SOVEREIGNTY] Remote origin successfully removed."
  fi
else
    echo ">> [SOVEREIGNTY] No .git directory to check."
fi
echo ">> [SOVEREIGNTY] Task completed."
