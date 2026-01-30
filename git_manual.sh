#!/bin/bash

# --- THEME (Dracula/Btop Style) ---
CYAN="\033[38;5;117m"
PURPLE="\033[38;5;141m"
GREEN="\033[38;5;84m"
GREY="\033[38;5;103m"
RESET="\033[0m"

# Configuration
REMOTE="origin"

# Helper to print colored status
function status_msg {
    echo -e "${PURPLE}>>> ${CYAN}$1${RESET}"
}

# SAFE PUSH FUNCTION
# This function will NEVER delete files. It only detects errors and informs the user.
function attempt_push {
    PUSH_CMD="$1"
    TEMP_LOG=$(mktemp)

    # Run the push
    eval "$PUSH_CMD" 2>&1 | tee "$TEMP_LOG"
    GIT_EXIT_CODE=${PIPESTATUS[0]}

    if [ $GIT_EXIT_CODE -ne 0 ]; then
        if grep -q "exceeds GitHub's file size limit" "$TEMP_LOG"; then
            echo ""
            echo -e "${PURPLE}!!! LARGE FILE ERROR DETECTED !!!${RESET}"
            LARGE_FILES=$(grep "exceeds GitHub's file size limit" "$TEMP_LOG" | awk '{print $4}')
            echo -e "${GREY}The following files are blocking the push (exceed 99MB):${RESET}"
            echo -e "${CYAN}$LARGE_FILES${RESET}"
            echo ""
            echo -e "${GREY}INSTRUCTION: Add these files to .gitignore or remove them from history manually.${RESET}"
            echo -e "${GREY}This script will NOT modify your files or history to fix this.${RESET}"
        fi
    fi
    rm "$TEMP_LOG"
}

# AUTO-IGNORE LARGE FILES
function check_large_files {
    status_msg "Scanning for files > 50MB..."
    # Find files > 50MB, excluding .git directory
    find . -type f -size +50M -not -path "./.git/*" | while read -r file; do
        # Remove leading ./ for cleaner gitignore entries
        clean_file="${file#./}"
        
        # Check if already ignored
        if ! git check-ignore -q "$clean_file"; then
            echo -e "${PURPLE}Blocking Large File: ${CYAN}$clean_file${RESET}"
            
            # Check if entry already exists in .gitignore (text match) to avoid duplicates
            if ! grep -Fxq "$clean_file" .gitignore; then
                echo "$clean_file" >> .gitignore
                echo -e "${GREEN}  -> Added to .gitignore${RESET}"
            else
                echo -e "${GREY}  -> Already in .gitignore (but likely tracked)${RESET}"
            fi
            
            # Unstage it if it was accidentally staged
            git reset HEAD "$clean_file" &>/dev/null
        fi
    done
}

while true; do
    # Dynamic Branch Detection
    BRANCH=$(git branch --show-current)
    
    clear
    echo -e "${PURPLE}==========================================${RESET}"
    echo -e "${CYAN}   ANVILOS CI/CD ${GREY}|${GREEN} WORKFLOW MANAGER${RESET}"
    echo -e "${GREY}   Current Branch: ${PURPLE}$BRANCH${RESET}"
    echo -e "${PURPLE}==========================================${RESET}"
    
    echo -e "${GREEN}1.${GREY} New Branch      ${CYAN}(Start Feature)${RESET}"
    echo -e "${GREEN}2.${GREY} Git Add         ${CYAN}(Stage Changes)${RESET}"
    echo -e "${GREEN}3.${GREY} Git Commit      ${CYAN}(Save Work)${RESET}"
    echo -e "${GREEN}4.${GREY} Git Push        ${CYAN}(Upload Feature)${RESET}"
    echo -e "${GREEN}5.${GREY} Git PR          ${CYAN}(Create Pull Request)${RESET}"
    echo -e "${GREEN}6.${GREY} Git Merge       ${CYAN}(Merge PR & Sync Main)${RESET}"
    echo -e "${GREEN}7.${GREY} Git Status      ${CYAN}(Check State)${RESET}"
    echo -e "${PURPLE}------------------------------------------${RESET}"
    echo -e "${GREEN}Q.${GREY} Quit${RESET}"
    echo -e "${PURPLE}------------------------------------------${RESET}"
    
    echo -e -n "${CYAN}Select an action: ${RESET}"
    read choice

    case $choice in
        1)
            status_msg "Creating New Branch"
            echo -e -n "${GREEN}Enter new branch name: ${RESET}"
            read new_branch
            if [ -n "$new_branch" ]; then
                git checkout -b "$new_branch"
                status_msg "Switched to: $new_branch"
            else
                echo -e "${PURPLE}Aborted.${RESET}"
            fi
            ;;
        2)
            check_large_files
            status_msg "Executing: git add ."
            git add . --verbose
            ;;
        3)
            status_msg "Executing: git commit"
            echo -e -n "${GREEN}Enter commit message: ${RESET}"
            read msg
            if [ -n "$msg" ]; then
                git commit -m "$msg"
            else
                echo -e "${PURPLE}Commit aborted: No message provided.${RESET}"
            fi
            ;;
        4)
            status_msg "Executing: git push (Setting Upstream)"
            attempt_push "git push -u $REMOTE $BRANCH"
            ;;
        5)
            status_msg "Creating Pull Request"
            if command -v gh &> /dev/null; then
                if ! gh pr create --fill; then
                     echo ""
                     echo -e "${PURPLE}!!! PR CREATION FAILED !!!${RESET}"
                     echo -e "${GREY}If you see 'Resource not accessible', run:${RESET}"
                     echo -e "${CYAN}gh auth refresh -s repo,workflow${RESET}"
                fi
            else
                echo -e "${PURPLE}Error: GitHub CLI (gh) not installed.${RESET}"
            fi
            ;;
        6)
            status_msg "Merging Pull Request & Syncing"
            if command -v gh &> /dev/null; then
                gh pr merge --merge --delete-branch
                if [ $? -eq 0 ]; then
                    status_msg "PR Merged. Switching to main and syncing..."
                    git checkout main || git checkout master
                    git pull origin main || git pull origin master
                else
                     echo -e "${PURPLE}Merge failed or cancelled.${RESET}"
                fi
            else
                echo -e "${PURPLE}Error: GitHub CLI (gh) not installed.${RESET}"
            fi
            ;;
        7)
            status_msg "Git Status"
            git status
            ;;
        [qQ])
            echo -e "${CYAN}Exiting...${RESET}"
            exit 0
            ;;
        *)
            echo -e "${PURPLE}Invalid option.${RESET}"
            ;;
    esac

    echo ""
    echo -e -n "${GREY}Press [Enter] to return to menu...${RESET}"
    read
done