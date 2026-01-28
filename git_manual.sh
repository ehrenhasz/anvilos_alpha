#!/bin/bash
# Auto-detect current branch
BRANCH=$(git rev-parse --abbrev-ref HEAD 2>/dev/null || echo "main")
REMOTE="origin"
CYAN="\033[38;5;117m"
PURPLE="\033[38;5;141m"
GREEN="\033[38;5;84m"
GREY="\033[38;5;103m"
RESET="\033[0m"
function status_msg {
    echo -e "${PURPLE}>>> ${CYAN}$1${RESET}"
}
function attempt_push {
    PUSH_CMD="$1"
    TEMP_LOG=$(mktemp)
    eval "$PUSH_CMD" 2>&1 | tee "$TEMP_LOG"
    GIT_EXIT_CODE=${PIPESTATUS[0]}
    if [ $GIT_EXIT_CODE -ne 0 ]; then
        if grep -q "exceeds GitHub's file size limit" "$TEMP_LOG"; then
            echo ""
            echo -e "${PURPLE}!!! LARGE FILE ERROR DETECTED !!!${RESET}"
            LARGE_FILES=$(grep "exceeds GitHub's file size limit" "$TEMP_LOG" | awk '{print $4}')
            echo -e "${GREY}The following files are blocking the push:${RESET}"
            echo -e "${CYAN}$LARGE_FILES${RESET}"
            echo ""
            echo -e "${GREY}These files are likely buried in your commit history.${RESET}"
            echo -e "${GREY}I will perform a DEEP CLEAN: Undo all local commits (keeping files safe), unstage these files, and re-commit.${RESET}"
            echo -e -n "${GREEN}Apply DEEP CLEAN fix now? (y/n): ${RESET}"
            read fix_confirm
            if [[ $fix_confirm == [yY] || $fix_confirm == [yY][eE][sS] ]]; then
                status_msg "Resetting local history to match remote (files are safe)..."
                git reset --soft $REMOTE/$BRANCH
                for f in $LARGE_FILES; do
                    git reset HEAD "$f"
                    echo "$f" >> .gitignore
                    echo -e "${CYAN}Removed $f from commit list and added to .gitignore${RESET}"
                done
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
    clear
    echo -e "${PURPLE}==========================================${RESET}"
    echo -e "${CYAN}   ANVILOS ALPHA ${GREY}|${GREEN} GIT CONTROL CENTER${RESET}"
    echo -e "${GREY}   Current Branch: ${PURPLE}$BRANCH${RESET}"
    echo -e "${PURPLE}==========================================${RESET}"
    echo -e "${GREEN}1.${GREY} Git Add ${CYAN}(Stage all changes)${RESET}"
    echo -e "${GREEN}2.${GREY} Git Commit ${CYAN}(Save staged changes)${RESET}"
    echo -e "${GREEN}3.${GREY} Git Push ${CYAN}(Standard - Upload to GitHub)${RESET}"
    echo -e "${GREEN}4.${GREY} Git Push FORCE ${PURPLE}(Local is Source of Truth)${RESET}"
    echo -e "${GREEN}5.${GREY} Git Merge ${CYAN}(Merge another branch)${RESET}"
    echo -e "${GREEN}6.${GREY} Git Clean ${CYAN}(Nuke untracked files)${RESET}"
    echo -e "${GREEN}7.${GREY} New Branch ${CYAN}(Create & Switch)${RESET}"
    echo -e "${GREEN}8.${GREY} Status Check ${CYAN}(Show state)${RESET}"
    echo -e "${GREEN}9.${GREY} Manual Deep Reset ${PURPLE}(Use if auto-fix fails)${RESET}"
    echo -e "${PURPLE}------------------------------------------${RESET}"
    echo -e "${GREEN}Q.${GREY} Quit${RESET}"
    echo -e "${PURPLE}------------------------------------------${RESET}"
    echo -e -n "${CYAN}Select an action: ${RESET}"
    read choice
    case $choice in
        1)
            status_msg "Executing: git add ."
            git add . --verbose
            ;;
        2)
            status_msg "Executing: git commit"
            echo -e -n "${GREEN}Enter commit message: ${RESET}"
            read msg
            if [ -n "$msg" ]; then
                git commit -m "$msg"
            else
                echo -e "${PURPLE}Commit aborted: No message provided.${RESET}"
            fi
            ;;
        3)
            status_msg "Executing: git push $REMOTE $BRANCH"
            attempt_push "git push $REMOTE $BRANCH"
            ;;
        4)
            status_msg "Executing: git push $REMOTE $BRANCH --force"
            echo -e "${PURPLE}WARNING: This overwrites remote history with local!${RESET}"
            echo -e -n "${GREEN}Are you sure? (y/n): ${RESET}"
            read confirm
            if [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]]; then
                attempt_push "git push $REMOTE $BRANCH --force"
            else
                echo "Force push aborted."
            fi
            ;;
        5)
            status_msg "Executing: git merge"
            echo -e -n "${GREEN}Enter branch to merge into $BRANCH: ${RESET}"
            read merge_branch
            if [ -n "$merge_branch" ]; then
                git merge "$merge_branch"
            else
                echo "Merge aborted."
            fi
            ;;
        6)
            status_msg "Checking for untracked files..."
            CLEAN_DRY_RUN=$(git clean -nfd)
            if [ -z "$CLEAN_DRY_RUN" ]; then
                echo -e "${GREEN}Repo is clean. No untracked files to delete.${RESET}"
            else
                echo -e "${PURPLE}The following files are untracked and will be DELETED:${RESET}"
                echo -e "${RED}$CLEAN_DRY_RUN${RESET}"
                echo ""
                echo -e -n "${GREEN}DELETE these files? (y/n): ${RESET}"
                read confirm
                if [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]]; then
                    status_msg "Executing: git clean -fd"
                    git clean -fd
                else
                    echo "Clean aborted."
                fi
            fi
            ;;
        7)
            status_msg "Creating New Branch"
            echo -e -n "${GREEN}Enter new branch name: ${RESET}"
            read new_branch
            if [ -n "$new_branch" ]; then
                git checkout -b "$new_branch"
                BRANCH="$new_branch"
                status_msg "Switched to new branch: $BRANCH"
            else
                echo "Branch creation aborted."
            fi
            ;;
        8)
            status_msg "Executing: git status"
            git status
            ;;
        9)
            status_msg "MANUAL DEEP RESET"
            echo -e "${GREY}This resets your commit history to match Origin, but keeps your file changes.${RESET}"
            echo -e -n "${GREEN}Run Deep Reset? (y/n): ${RESET}"
            read confirm
            if [[ $confirm == [yY] || $confirm == [yY][eE][sS] ]]; then
                git reset --soft $REMOTE/$BRANCH
                echo -e "${CYAN}History reset. You can now use Option 1 (Add) and 2 (Commit) to rebuild your commit cleanly.${RESET}"
            fi
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
