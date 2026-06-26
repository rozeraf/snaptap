#!/usr/bin/env bash

SERVICE=/etc/systemd/system/snaptap.service

sudo systemctl disable --now snaptap 2>/dev/null || true
sudo rm -f "$SERVICE"
sudo systemctl daemon-reload

echo "SnapTap stopped and removed from autostart."
