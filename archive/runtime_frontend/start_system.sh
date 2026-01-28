PROJECT_ROOT="/mnt/anvil_temp/dnd_vm"
PYTHON_EXEC="/usr/bin/python3"
LOG_DIR="$PROJECT_ROOT/logs"
mkdir -p "$LOG_DIR"
echo "Starting Interceptor Bridge..."
nohup $PYTHON_EXEC "$PROJECT_ROOT/runtime/services/interceptor_bridge.py" > "$LOG_DIR/interceptor_bridge.log" 2>&1 &
INTERCEPTOR_PID=$!
echo "Interceptor PID: $INTERCEPTOR_PID"
echo "Starting Dashboard Server..."
cd "$PROJECT_ROOT/runtime"
exec $PYTHON_EXEC dashboard_server.py > "$LOG_DIR/dashboard_server.log" 2>&1
