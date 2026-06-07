@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

taskkill /f /im snaptap.exe >nul 2>&1
schtasks /delete /tn "SnapTap" /f >nul 2>&1

echo SnapTap stopped and removed from autostart.
timeout /t 2 >nul
