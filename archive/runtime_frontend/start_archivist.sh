#!/bin/bash

# Ensure log directory exists
mkdir -p logs

# Kill existing instance if any
pkill -f "runtime/library_archivist.py"

# Start in background
nohup python3 runtime/library_archivist.py > logs/archivist.log 2>&1 &

PID=$!
echo "Archivist started with PID $PID. Logs at logs/archivist.log"
echo $PID > runtime/archivist.pid
