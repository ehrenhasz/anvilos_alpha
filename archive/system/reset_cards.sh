#!/bin/bash
echo "Resetting Failed/Stuck Cards..."
mv cards/failed/* cards/queue/ 2>/dev/null
mv cards/processing/* cards/queue/ 2>/dev/null
echo "Queue reset complete."
