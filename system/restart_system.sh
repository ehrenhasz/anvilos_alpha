#!/bin/bash
echo "Resetting Failed Cards..."
mv cards/failed/* cards/queue/ 2>/dev/null
echo "Restarting Service..."
pkill -f cardreader.py
nohup python3 -u cardreader.py > logs/service.log 2>&1 &
echo "Done."
./dashboard.sh
