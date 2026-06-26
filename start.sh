#!/usr/bin/env bash
set -e

SELF=$(cd "$(dirname "$0")" && pwd)
EXE="$SELF/snaptap-linux"
SERVICE=/etc/systemd/system/snaptap.service

if [[ ! -x "$EXE" ]]; then
    echo "snaptap-linux not found or not executable at $EXE"
    echo "Build first: g++ -O2 -std=c++17 -o snaptap-linux snaptap-linux.cpp"
    exit 1
fi

sudo tee "$SERVICE" > /dev/null << EOF
[Unit]
Description=SnapTap kernel-level snap tap
After=multi-user.target

[Service]
ExecStart=$EXE
Restart=on-failure
RestartSec=2
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
EOF

sudo systemctl daemon-reload
sudo systemctl enable --now snaptap

echo "SnapTap started and added to autostart."
