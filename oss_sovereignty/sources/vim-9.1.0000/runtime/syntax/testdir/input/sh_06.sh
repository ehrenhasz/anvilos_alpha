DebugScript=set
[ -t 0 ] && echo "Please wait ... \c" >`tty`
Usage () {
VariableName="${BasicConfigName}_*"
echo "CDPATH="${CDPATH}
eval BackupMethod=\$mess09${BackupType}B
case $BackupType in
  3)   DefaultDevice=$MountDevice    ;;
  1|2) DefaultDevice=$TapeDrive      ;;
esac
for Variable in DefaultExclude DefaultFindOption DoNotBackupList
do
    eval VarValue=\$$Variable
    VarValue=`echo $VarValue | FoldS 53 | sed "2,\\$s/^/$Tab$Tab$Tab/"`
    eval $Variable=\$VarValue
done
echo "
Usage:  $ScriptName [-Options]
Options List:
        -v              The current version of '$ScriptName'
        -h  | -H | ?    Display this list
"
} 
ExecuteFbackup () { 
[ "$DebugScript" ]    && set -x || set +x
cd $cwd
} 
Usage
Exit $Result
