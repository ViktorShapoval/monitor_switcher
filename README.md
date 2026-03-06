# monitor_switcher

A lightweight Windows command-line tool to switch the primary display. Single C++ file, no dependencies beyond the Win32 API.

## TLDR

```
monitor_switcher.exe --list
monitor_switcher.exe --toggle
monitor_switcher.exe --set-primary <index>
monitor_switcher.exe --set-primary <device_name>
```

## Why

Windows 11 has no built-in command-line way to change the primary monitor. Tools like nircmd don't handle it reliably on modern Windows, and PowerShell has no native cmdlet for it. This tool calls `ChangeDisplaySettingsEx` with `CDS_SET_PRIMARY` directly, which is the only fully supported method.

It preserves your existing resolution, refresh rate, color depth, and all other display settings — only the primary assignment is changed.

## Building

**MSVC** (Developer Command Prompt for Visual Studio):

```
cl /O2 monitor_switcher.cpp user32.lib
```

**MinGW / g++**:

```
g++ -O2 -o monitor_switcher.exe monitor_switcher.cpp -lgdi32 -luser32
```

No external libraries or SDKs needed beyond a standard Windows C++ toolchain.

## Usage

### `--list`

List all active displays with their index, device name, resolution, refresh rate, position, and primary status.

```
> monitor_switcher.exe --list
Displays (2 found):

  [1]  \\.\DISPLAY1  3840x2160 @ 240Hz  pos(0,0)  [PRIMARY]
  [2]  \\.\DISPLAY2  2560x1600 @ 120Hz  pos(704,2160)
```

### `--toggle`

Switch primary to whichever of your two displays is not currently primary. Requires exactly 2 active displays.

```
> monitor_switcher.exe --toggle
```

### `--set-primary`

Set a specific display as primary, by 1-based index or device name:

```
> monitor_switcher.exe --set-primary 2
> monitor_switcher.exe --set-primary \\.\DISPLAY2
```

## How it works

1. Enumerates all active displays via `EnumDisplayDevices` / `EnumDisplaySettings`.
2. Computes a position offset so the new primary sits at (0, 0) and all other monitors shift accordingly.
3. Stages each display with `ChangeDisplaySettingsExA` using `CDS_UPDATEREGISTRY | CDS_NORESET`, preserving the full `DEVMODE` returned by `ENUM_CURRENT_SETTINGS` (the new primary is staged first).
4. Commits all changes atomically with a final `ChangeDisplaySettingsExA(NULL, NULL, NULL, 0, NULL)` call.

Preserving the original `dmFields` from `EnumDisplaySettings` (rather than setting them manually) is key to avoiding unintended resets of refresh rate or resolution on some drivers.

## License

MIT