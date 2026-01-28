#!/bin/bash

# Script to install Titanium Warden service

# Copy the service file to the systemd directory
cp /ANVIL_OS/services/titanium_warden.service /etc/systemd/system/

# Enable and start the service
systemctl enable titanium_warden.service
systemctl start titanium_warden.service

echo "Titanium Warden service installed and started."
