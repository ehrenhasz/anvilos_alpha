mkdir -p logs
pkill -f "runtime/library_archivist.py"
nohup python3 runtime/library_archivist.py > logs/archivist.log 2>&1 &
PID=$!
echo "Archivist started with PID $PID. Logs at logs/archivist.log"
echo $PID > runtime/archivist.pid
