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

# INTELLIGENT PUSH FUNCTION (Preserved Logic)
function attempt_push {
    PUSH_CMD="$1"
    TEMP_LOG=$(mktemp)

    # Run the push, show output to user, but also save to temp file
    eval "$PUSH_CMD" 2>&1 | tee "$TEMP_LOG"
    GIT_EXIT_CODE=${PIPESTATUS[0]}

    if [ $GIT_EXIT_CODE -ne 0 ]; then
        # Check if failure was due to file size
        if grep -q "exceeds GitHub's file size limit" "$TEMP_LOG"; then
            echo ""
            echo -e "${PURPLE}!!! LARGE FILE ERROR DETECTED !!!${RESET}"
            
            # Extract filenames
            LARGE_FILES=$(grep "exceeds GitHub's file size limit" "$TEMP_LOG" | awk '{print $4}')
            
            echo -e "${GREY}The following files are blocking the push:${RESET}"
            echo -e "${CYAN}$LARGE_FILES${RESET}"
            echo ""
            echo -e "${GREY}These files are likely buried in your commit history.${RESET}"
            echo -e "${GREY}I will perform a DEEP CLEAN: Undo all local commits (keeping files safe), unstage these files, and re-commit.${RESET}"
            
            echo -e -n "${GREEN}Apply DEEP CLEAN fix now? (y/n): ${RESET}"
            read fix_confirm
            
            if [[ $fix_confirm == [yY] || $fix_confirm == [yY][eE][sS] ]]; then
                # 1. Soft reset everything to match remote (Safe: keeps file changes)
                status_msg "Resetting local history to match remote (files are safe)..."
                git reset --soft $REMOTE/$BRANCH
                
                # 2. Unstage and ignore specific files
                for f in $LARGE_FILES; do
                    # Unstage the file
                    git reset HEAD "$f"
                    # Add to gitignore
                    echo "$f" >> .gitignore
                    echo -e "${CYAN}Removed $f from commit list and added to .gitignore${RESET}"
                done
                
                # 3. Re-add everything else and commit
                git add .gitignore
                git commit -m "Update repo (Auto-removed large files)"
                
                status_msg "Deep Clean complete. Retrying push..."
                eval "$PUSH_CMD"
            else
                echo -e "${GREY}Fix cancelled.${RESET}"
            fi
        fi
    fi
    rm "$TEMP_LOG"
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
                gh pr create --fill
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
