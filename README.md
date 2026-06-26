# snaptap

Kernel-level snap tap. Works with DirectInput and RawInput games — input is intercepted before any application sees it.

> **Warning:** kernel-level input interception may trigger anti-cheat bans in some games.
> Counter-Strike 2 (VAC + CS2 anti-cheat) is known to flag both Interception (Windows) and evdev-grab (Linux) tools.
> Use at your own risk. Safe for games without kernel anti-cheat (e.g. most racing sims).

## How it works

When two opposing keys are held simultaneously, the older one is synthetically released and restored on key-up.
Pairs are fully independent: holding `Up` does not affect `Left`/`Right` snap tap.

Pairs: `A/D`, `W/S`, `Left/Right`, `Up/Down`.

---

## Windows

Uses the [Interception](https://github.com/oblitum/Interception/releases) driver — hooks into the keyboard driver stack below DirectInput/RawInput.

### Prerequisites

**Build machine (Linux)**

- `mingw-w64-gcc` - cross-compiler targeting Windows x86-64

```zsh
paru -S mingw-w64-gcc
```

**Target machine (Windows)**

- Interception driver installed and system rebooted
- `interception.dll` next to `snaptap.exe`

Install the driver (run as Administrator, then reboot):

```cmd
install-interception.exe /install
```

### Build

```zsh
x86_64-w64-mingw32-g++ -O2 -std=c++17 -o snaptap.exe snaptap.cpp \
    -lkernel32 -luser32 \
    -static-libgcc -static-libstdc++ \
    -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
```

### Deploy

```
snaptap.exe
interception.dll
start.bat
stop.bat
```

`libwinpthread-1.dll` is statically linked into the binary with the flags above, so it is not needed separately.
If you build without `-Wl,-Bstatic -lwinpthread -Wl,-Bdynamic`, copy it from
`/usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll` on the build machine.

- `start.bat` - starts snaptap, adds to autostart via Task Scheduler (requires UAC prompt)
- `stop.bat` - kills process, removes from autostart (requires UAC prompt)

The process runs hidden (`--hidden` flag). No tray icon, no console window.

---

## Linux

Uses evdev + uinput — grabs real keyboard devices exclusively and forwards events through a virtual device.
Works on any Linux system with Wayland or X11 (tested on Niri, also compatible with Hyprland, Sway, GNOME, KDE).

### Prerequisites

No external libraries — only kernel headers, which are always available.

Allow access to input devices without root (relogin after):

```zsh
sudo usermod -aG input,uinput "$USER"
```

Ensure the uinput module is loaded:

```zsh
sudo modprobe uinput
# to make it persistent:
echo uinput | sudo tee /etc/modules-load.d/uinput.conf
```

Optionally add a udev rule so `/dev/uinput` is accessible to the `uinput` group:

```zsh
echo 'KERNEL=="uinput", GROUP="uinput", MODE="0660"' \
    | sudo tee /etc/udev/rules.d/99-uinput.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

### Build

```zsh
g++ -O2 -std=c++17 -o snaptap-linux snaptap-linux.cpp
```

### Deploy

```
snaptap-linux
start.sh
stop.sh
```

```zsh
chmod +x start.sh stop.sh snaptap-linux
./start.sh   # installs systemd service, starts immediately
./stop.sh    # stops and removes from autostart
```

- `start.sh` - installs and starts a systemd system service, enables autostart on boot
- `stop.sh` - stops the service and removes it

The service runs as root (required for EVIOCGRAB). Logs are in journald: `journalctl -u snaptap -f`.
