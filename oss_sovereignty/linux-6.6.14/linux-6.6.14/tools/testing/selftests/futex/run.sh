USE_COLOR=0
tput setf 7 || tput setaf 7
if [ $? -eq 0 ]; then
    USE_COLOR=1
    tput sgr0
fi
export USE_COLOR
(cd functional; ./run.sh)
