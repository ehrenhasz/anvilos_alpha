# --- BIGIRON BTOP THEME ---
export BTOP_CYAN="\[\033[38;5;117m\]"
export BTOP_PURPLE="\[\033[38;5;141m\]"
export BTOP_GREEN="\[\033[38;5;84m\]"
export BTOP_GREY="\[\033[38;5;103m\]"
export BTOP_RESET="\[\033[0m\]"

# High-fidelity prompt: [time] user@host:cwd $
PS1="${BTOP_GREY}[ \t ] ${BTOP_CYAN}\u${BTOP_GREY}@${BTOP_CYAN}\h${BTOP_GREY}:${BTOP_PURPLE}\w ${BTOP_GREEN}\$ ${BTOP_RESET}"

# Dracula-inspired LS_COLORS
export LS_COLORS="di=38;5;141:fi=0:ln=38;5;117:pi=5:so=5:bd=5:cd=5:or=31:mi=0:ex=38;5;84:*.py=38;5;228:*.js=38;5;228:*.json=38;5;117"

alias ls='ls --color=auto'
alias grep='grep --color=auto'
# --------------------------
