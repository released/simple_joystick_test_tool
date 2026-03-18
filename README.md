# JoyStickTestTool

Windows MFC tool for validating USB HID joystick/gamepad input reports.

## Current Scope

This project is **RX-focused**.

Implemented:
- HID scan and device selection
- Connect/Disconnect
- Continuous input report capture
- Raw RX payload grid (10 rows x 64 bytes)
- Row-level `Copy` button for each RX row (copies row hex to clipboard)
- Descriptor-based visualization:
  - `X/Y`
  - `Z/Rz`
  - `POV`
  - `Buttons 1-N` (dynamic)
- Right-side gamepad icon panel (vector draw, no image file dependency) with live state highlighting
- Raw byte to field mapping table from HID descriptor
- `Show All HID` filter toggle
- `Auto reconnect`
- Optional RX log file with timestamp option

Not included in current UI:
- TX scheduler / TX command editor
- Handshake or Echo mode controls

## Build

Recommended:

```bat
scripts\build_release.bat
```

Alternative:

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

Both scripts build `Release|x64` and (unless explicitly skipped) launch:
- `build\JoyStickTestTool.exe`

App icon source:
- `src\res\hid.ico` (embedded as `IDI_APP_ICON`) controls the icon shown in File Explorer, taskbar, and window top-left corner.

## Quick Start

1. Run `scripts\build_release.bat`.
2. Wait for `build\JoyStickTestTool.exe` to auto-launch.
3. Click `Scan Device` if needed.
4. Select a device in the combo box.
5. Optional: toggle `Show All HID` if your target is filtered out.
6. Click `Connect`.
7. Move stick / POV / buttons and verify:
   - visualizer updates
   - RX grid updates
   - status bar `RX` and `JOY` update

## Device Selection Behavior (Important)

When disconnected, changing the combo selection will probe that device descriptor and update visual preview.

This means:
- button display count (`Buttons 1-N`) changes with selected device
- mapping table updates per selected device
- no connect click required for preview

Note:
- descriptor-driven joystick visualization is applied only to joystick/gamepad usage (`UsagePage 0x0001`, `Usage 0x0004/0x0005`)
- with `Show All HID` enabled, selecting non-joystick HID will not drive joystick button-count rendering

## UI Notes

- Default scan filter keeps only:
  - `UsagePage=0x0001`
  - `Usage=0x0004 (Joystick)` or `0x0005 (Gamepad)`
- `Show All HID` disables this filter.
- RX panel order:
  - Visualizer
  - Raw Byte -> Field Mapping
  - Log/Clear controls
  - RX grid
- each RX row has a `Copy` button at the row end
- DeviceBar connection-status dot/text now uses deduplicated updates + partial redraw to reduce flicker while joystick data is active.

Visualizer notes:
- Left visualizer now has a title row (`Joystick Visualizer`) to match right-side panel style.
- Left and right visualizer panels are split by center line (about 50/50 width each) in normal window size.
- Left side keeps existing `X/Y`, `Z/Rz`, `POV`, `Buttons` sections.
- Left-side 4 sections are now equal-width distributed to reduce unused blank area.
- `Z/Rz` vertical bars now have wider gap and show each value text above the bar, centered to that bar.
- `Z/Rz` bar height is reduced so bars and labels stay fully inside the left titled panel.
- Right side shows `Gamepad Mapping` icon panel as the right half panel (very narrow width may fallback).
- Right-side `Gamepad Mapping` panel includes a checkbox `Swap DPad <-> L3`.
- Default checkbox state is checked: `X/Y -> UDLR`, `POV -> L3`.
- Unchecked state restores original mapping: `POV -> UDLR`, `X/Y -> L3`.
- Right panel no longer depends on `example.png`.
- RX visualizer repaints with double-buffering to reduce flicker during live input.
- Right icon panel uses larger stick rings/movement range for clearer analog travel.
- `SELECT` / `START` icon boxes are enlarged to keep labels readable.

Icon mapping details:
- D-pad operation highlights both POV circle and icon D-pad.
- Diagonal POV is shown as two directions active at the same time (for example `Up + Right`).
- Left stick operation highlights both X/Y box and icon left stick.
- Right stick operation highlights both Z/Rz bars and icon right stick.
- If `Swap DPad <-> L3` is checked, right icon panel uses `X/Y` to drive D-pad and uses `POV` to drive `L3`.
- ABXY icon layout uses common Xbox-style diamond (`Y` top, `A` bottom, `X` left, `B` right).
- Buttons highlight both button circles and icon controls:
  - B1=`X`, B2=`A`, B3=`B`, B4=`Y`
  - B5=`L1`, B6=`R1`, B7=`L2`, B8=`R2`
  - B9=`SELECT`, B10=`START`, B11=`L3`, B12=`R3`

## Logs

When `Save to log` is checked, file is created in exe directory:
- `YYYYMMDD_HHMMSS_hid_rx.log`

Line format:
- no timestamp: `[RX] [..hex..]`
- with timestamp: `[HH:MM:SS.mmm] [RX] [..hex..]`

## Configuration

Main config file:
- `JoyStickTestTool.ini`

Actively used now:
- last selected device (`Path`, `VID`, `PID`, `PathHash`)
- `LogEnabled`
- `TimestampEnabled`

Note:
- Some legacy TX-related keys may still exist in INI for compatibility, but current UI does not expose TX features.

## Status Bar Behavior

- Pane text includes `Conn`, `Err`, `RX`, `JOY`.
- Width allocation prioritizes the rightmost `JOY` pane.
- If width is constrained, left panes are compressed first so `JOY:` trailing text is less likely to be clipped.

## Maintenance: Document Sync Template

Every feature update must also update:
- `AGENTS.md`
- `README.md`
- `SELF_CHECK.md`

Use the fixed `DOC_SYNC_RECORD` template in `AGENTS.md` section:
- `11. Document Sync Template (Required)`

![image](https://github.com/released/JoyStickTestTool/blob/main/JoyStickTestTool.jpg) 

