TARGET_DIR="oss_sovereignty/bld_01_GCC"
if [ ! -d "${TARGET_DIR}" ]; then
  echo "Target directory ${TARGET_DIR} does not exist. Creating it."
  mkdir -p "${TARGET_DIR}"
fi
extract_gcc_download_command() {
  grep -oP 'wget .*gcc-[0-9.]*\.tar\.gz' anvil.build.sh
}
download_command=
if [ -z "${download_command}" ]; then
  echo "Error: Could not find GCC download command in anvil.build.sh"
  exit 1
fi
filename=${download_command}
full_path="${TARGET_DIR}/${filename}"
if [ ! -f "${full_path}" ]; then
  echo "Source code not found. Downloading GCC..."
  cd "${TARGET_DIR}" || exit 1
  eval "${download_command}" # Execute the download command
  cd .. || exit 1 # Return to the original directory
else
  echo "Source code already present in ${TARGET_DIR}. Skipping download."
fi
extract_gcc_tar_command() {
  grep -oP 'tar -.*gcc-[0-9.]*\.tar\.gz' anvil.build.sh
}
tar_command=
extracted_dir=
extracted_dir="/"
if [ ! -d "" ]; then
  echo "Extracting archive..."
  cd "" || exit 1
  eval ""
  cd .. || exit 1
fi
if [ -d "${extracted_dir}/.git" ]; then
  echo "Found a git repository. Disconnecting from origin..."
  cd "${extracted_dir}" || exit 1
  git remote remove origin
  if git remote get-url origin 2>/dev/null; then
    echo "Error: Failed to remove origin remote."
    exit 1
  else
    echo "Successfully disconnected from origin."
  fi
  cd ../.. || exit 1
else
  echo "Not a git repository."
fi
echo "Repository status for ${extracted_dir}:"
cd "${extracted_dir}" || exit 1
if git remote get-url origin 2>/dev/null; then
  echo "Remote origin still exists."
else
  echo "No remote origin found."
fi
cd ../.. || exit 1
exit 0
