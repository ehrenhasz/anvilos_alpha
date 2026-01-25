#!/bin/bash

# Set the target directory
TARGET_DIR="oss_sovereignty/bld_01_GCC"

# Check if the target directory exists
if [ ! -d "${TARGET_DIR}" ]; then
  echo "Target directory ${TARGET_DIR} does not exist. Creating it."
  mkdir -p "${TARGET_DIR}"
fi

# Function to extract the GCC download command from anvil.build.sh
extract_gcc_download_command() {
  grep -oP 'wget .*gcc-[0-9.]*\.tar\.gz' anvil.build.sh
}

# Extract the GCC download command
download_command=

# Check if the download command is empty
if [ -z "${download_command}" ]; then
  echo "Error: Could not find GCC download command in anvil.build.sh"
  exit 1
fi

# Extract the filename from the download command
filename=${download_command}

# Construct the full path to the downloaded file
full_path="${TARGET_DIR}/${filename}"

# Check if the source code is already present (checking for the archive)
if [ ! -f "${full_path}" ]; then
  echo "Source code not found. Downloading GCC..."
  cd "${TARGET_DIR}" || exit 1
  eval "${download_command}" # Execute the download command
  cd .. || exit 1 # Return to the original directory
else
  echo "Source code already present in ${TARGET_DIR}. Skipping download."
fi

# Extract the tar command from anvil.build.sh
extract_gcc_tar_command() {
  grep -oP 'tar -.*gcc-[0-9.]*\.tar\.gz' anvil.build.sh
}

# Extract the GCC tar command
tar_command=

# Extract the directory name that will result from untarring the archive
extracted_dir=
extracted_dir="/"

# Check if the extracted directory already exists.  If not, extract.
if [ ! -d "" ]; then
  echo "Extracting archive..."
  cd "" || exit 1
  eval ""
  cd .. || exit 1
fi


# Check if it's a git repository and disconnect from origin
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

# Report the repository status (check for remote origin)
echo "Repository status for ${extracted_dir}:"
cd "${extracted_dir}" || exit 1
if git remote get-url origin 2>/dev/null; then
  echo "Remote origin still exists."
else
  echo "No remote origin found."
fi
cd ../.. || exit 1
exit 0
