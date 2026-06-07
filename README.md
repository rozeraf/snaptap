# snaptap

Kernel-level snap tap via the Interception driver. Works with DirectInput and RawInput games.

> **Warning:** kernel-level input interception may trigger anti-cheat bans in some games.
> Counter-Strike 2 (VAC + CS2 anti-cheat) is known to flag Interception-based tools.
> Use at your own risk. Safe for games without kernel anti-cheat.

## How it works

Interception hooks into the keyboard driver stack before any application sees input.
When two opposing keys are held simultaneously, the older one is released and restored
on key-up. Pairs are fully independent: holding `Up` does not affect `Left`/`Right` snap tap.

Pairs: `A/D`, `W/S`, `Left/Right`, `Up/Down`.

## Prerequisites

**Linux (build machine)**

- `mingw-w64-gcc` - cross-compiler targeting Windows x86-64

```zsh
paru -S mingw-w64-gcc
```

**Windows (target machine)**

- Interception driver installed and system rebooted
- `interception.dll` next to `snaptap.exe`

Both are from the same release: https://github.com/oblitum/Interception/releases

Install the driver (run as Administrator, then reboot):

```cmd
install-interception.exe /install
```

## Build

```zsh
x86_64-w64-mingw32-g++ -O2 -std=c++17 -o snaptap.exe snaptap.cpp \
    -lkernel32 -luser32 \
    -static-libgcc -static-libstdc++ \
    -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic
```

## Deploy

Put these files in one folder on Windows:

```
snaptap.exe
interception.dll
libwinpthread-1.dll
start.bat
stop.bat
```

`libwinpthread-1.dll` is part of the mingw-w64 runtime. On Arch it is at
`/usr/x86_64-w64-mingw32/bin/libwinpthread-1.dll`.

- `start.bat` - starts snaptap, adds to autostart via Task Scheduler (requires UAC prompt)
- `stop.bat` - kills process, removes from autostart (requires UAC prompt)

The process runs hidden (`--hidden` flag). No tray icon, no console window.
