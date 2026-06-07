@echo off
net session >nul 2>&1
if %errorlevel% neq 0 (
    powershell -Command "Start-Process -FilePath '%~f0' -Verb RunAs"
    exit /b
)

set EXE=%~dp0snaptap.exe

schtasks /create /tn "SnapTap" /tr "\"%EXE%\" --hidden" /sc onlogon /rl highest /f >nul
schtasks /run /tn "SnapTap" >nul

echo SnapTap started and added to autostart.
timeout /t 2 >nul
