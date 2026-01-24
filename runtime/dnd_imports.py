#!/usr/bin/env python3
import os
import shutil
import sys
import subprocess
import re
import logging
import hashlib
from pathlib import Path

# --- CONFIGURATION ---
BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SOURCE_DIR = os.path.join(BASE_DIR, "dnd_import")
LIBRARY_DIR = os.path.join(BASE_DIR, "DND_Library")
FAILED_DIR = os.path.join(SOURCE_DIR, "failed_imports")
UNSORTED_DIR = os.path.join(LIBRARY_DIR, "Unsorted")

# Extensions
ARCHIVE_EXTS = {'.zip', '.rar', '.cbr', '.cbz', '.7z', '.tar', '.gz'}

# Logging setup
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - [IMPORT] - %(levelname)s - %(message)s',
    handlers=[
        logging.StreamHandler(sys.stdout)
    ]
)
logger = logging.getLogger()

def setup_dirs():
    """Ensure necessary directories exist."""
    for d in [SOURCE_DIR, LIBRARY_DIR, FAILED_DIR, UNSORTED_DIR]:
        os.makedirs(d, exist_ok=True)

def sanitize_filename(filename):
    """
    Sanitizes a filename:
    - Replaces spaces with underscores.
    - Removes non-alphanumeric characters (except ._-).
    - Lowercases extension.
    """
    name, ext = os.path.splitext(filename)
    
    # Replace spaces with underscores
    name = name.replace(" ", "_")
    
    # Remove unwanted characters (keep alphanumeric, underscores, hyphens)
    name = re.sub(r'[^a-zA-Z0-9_\-]', '', name)
    
    # Ensure no double underscores
    name = re.sub(r'_+', '_', name)
    
    return f"{name}{ext.lower()}"

def build_library_index(path):
    """
    Builds a set of existing filenames in the library to detect duplicates.
    Returns: set(filenames)
    """
    logger.info("Building Library Index...")
    index = set()
    count = 0
    for root, _, files in os.walk(path):
        for file in files:
            index.add(file.lower())
            count += 1
    logger.info(f"Library Index built. {count} items found.")
    return index

def extract_archive(file_path):
    """
    Extracts an archive to a folder named after the archive.
    Returns the path to the extracted folder, or None if failed.
    """
    try:
        filename = os.path.basename(file_path)
        base_name = os.path.splitext(filename)[0]
        # Create a unique extraction dir to avoid collisions
        extract_dir = os.path.join(os.path.dirname(file_path), f"_extracted_{base_name}")
        
        if os.path.exists(extract_dir):
            shutil.rmtree(extract_dir)
        os.makedirs(extract_dir)
        
        ext = os.path.splitext(filename)[1].lower()
        
        logger.info(f"Extracting {filename} to {extract_dir}...")
        
        if ext in ['.zip', '.cbz']:
            subprocess.run(['unzip', '-q', '-o', file_path, '-d', extract_dir], check=True)
        elif ext in ['.rar', '.cbr']:
            # unrar x -o+ file.rar dest_dir/
            subprocess.run(['unrar', 'x', '-o+', '-inul', file_path, extract_dir], check=True)
        elif ext in ['.tar', '.gz']:
            subprocess.run(['tar', '-xf', file_path, '-C', extract_dir], check=True)
        elif ext == '.7z':
             # requiring 7z installed, assume unrar/unzip for now as checked
             # Fallback or error
             logger.warning(f"7z not explicitly checked, trying 7z command...")
             subprocess.run(['7z', 'x', '-y', f'-o{extract_dir}', file_path], check=True)
        
        return extract_dir
    except Exception as e:
        logger.error(f"Extraction failed for {file_path}: {e}")
        return None

def move_to_failed(file_path, reason="Unknown"):
    """Moves a file to the failed_imports directory."""
    try:
        dest = os.path.join(FAILED_DIR, os.path.basename(file_path))
        # Handle collision in failed folder
        if os.path.exists(dest):
            base, ext = os.path.splitext(os.path.basename(file_path))
            dest = os.path.join(FAILED_DIR, f"{base}_{hashlib.md5(os.urandom(4)).hexdigest()[:4]}{ext}")
            
        shutil.move(file_path, dest)
        logger.warning(f"Moved to FAILED: {os.path.basename(file_path)} ({reason})")
    except Exception as e:
        logger.error(f"CRITICAL: Could not move {file_path} to failed dir: {e}")

def process_directory(current_dir, library_index, root_source_dir):
    """
    Recursively processes a directory.
    - Sanitize filenames
    - Extract archives
    - Import regular files
    """
    # We walk using listdir to handle dynamic changes (like extraction) carefully
    # But recursion is safer for logic.
    
    # 1. Sanitize all filenames in this dir first
    files = os.listdir(current_dir)
    for f in files:
        full_path = os.path.join(current_dir, f)
        if os.path.isdir(full_path):
            if full_path == FAILED_DIR: continue # Skip failed dir
            # Sanitize dir name? Maybe later. Recurse.
            process_directory(full_path, library_index, root_source_dir)
            
            # Clean up empty dirs (except root source)
            if not os.listdir(full_path) and full_path != root_source_dir:
                os.rmdir(full_path)
                
        else:
            # It's a file. Sanitize name.
            sanitized = sanitize_filename(f)
            if sanitized != f:
                new_path = os.path.join(current_dir, sanitized)
                try:
                    os.rename(full_path, new_path)
                    logger.info(f"Sanitized: {f} -> {sanitized}")
                    full_path = new_path
                except OSError as e:
                    logger.error(f"Rename failed for {f}: {e}")
                    move_to_failed(full_path, "Rename Failed")
                    continue
            
            # Now process the (potentially renamed) file
            process_file(full_path, library_index)

def process_file(file_path, library_index):
    filename = os.path.basename(file_path)
    ext = os.path.splitext(filename)[1].lower()
    
    # CASE 1: Archive
    if ext in ARCHIVE_EXTS:
        extracted_path = extract_archive(file_path)
        if extracted_path:
            # Successfully extracted. 
            # Delete the archive file (Source).
            os.remove(file_path)
            logger.info(f"Archive extracted and removed: {filename}")
            
            # Now process the extracted contents
            process_directory(extracted_path, library_index, SOURCE_DIR)
        else:
            move_to_failed(file_path, "Extraction Failed")
            
    # CASE 2: Regular File
    else:
        # Check Duplicate
        if filename.lower() in library_index:
            logger.info(f"Duplicate found (Skipping/Deleting): {filename}")
            os.remove(file_path)
        else:
            # Determine relative path to preserve structure
            # If file is in dnd_import/A/B/file.pdf, we want DND_Library/Unsorted/A/B/file.pdf
            
            # Find the root of the import (which might be the recursion root or SOURCE_DIR)
            # We need to pass the import root down or calculate it.
            # Simpler: We know SOURCE_DIR is the base.
            
            try:
                rel_path = os.path.relpath(os.path.dirname(file_path), SOURCE_DIR)
            except ValueError:
                # Fallback if path is weird (e.g. symlinks or outside tree)
                rel_path = "."
            
            if rel_path == ".":
                target_subdir = UNSORTED_DIR
            else:
                # If path starts with _extracted_, we might want to flatten that specific layer 
                # or keep it. Let's keep it to be safe, or maybe strip the _extracted_ prefix if we want cleaner paths?
                # For now, preserve exactly.
                target_subdir = os.path.join(UNSORTED_DIR, rel_path)
            
            os.makedirs(target_subdir, exist_ok=True)
            dest_path = os.path.join(target_subdir, filename)
            
            # Handle collision in Target (same relative path and name)
            if os.path.exists(dest_path):
                base, ext = os.path.splitext(filename)
                dest_path = os.path.join(target_subdir, f"{base}_{hashlib.md5(os.urandom(4)).hexdigest()[:4]}{ext}")

            try:
                shutil.move(file_path, dest_path)
                library_index.add(filename.lower()) # Update index
                logger.info(f"Imported: {filename} -> {os.path.relpath(dest_path, LIBRARY_DIR)}")
            except Exception as e:
                move_to_failed(file_path, f"Move Error: {e}")

def main():
    setup_dirs()
    logger.info("Starting DND Import Pipeline...")
    
    # 1. Build Index
    lib_index = build_library_index(LIBRARY_DIR)
    
    # 2. Start Processing
    process_directory(SOURCE_DIR, lib_index, SOURCE_DIR)
    
    logger.info("Import Pipeline Completed.")

if __name__ == "__main__":
    main()
