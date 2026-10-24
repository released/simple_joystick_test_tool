# AGENTS.md
## Verified x64 size configuration (2026-10-25)

- Only x64 defines _AFX_NO_MFC_CONTROLS_IN_DIALOGS after standard/common/custom
  control review. Reassess before introducing CMFC controls; keep Win32 unchanged.
- Keep existing Release optimization, /GL, static /MT, /GS and PE security;
  never trade device/protocol features or installation requirements for fewer bytes.
- Validate hash-matched isolated Release/Debug startup/control/modal/close paths,
  scoped paired performance and actual clean; use no personal INI/device commands.
- Actual results/raw timing: SELF_CHECK.md and docs/verification/exe-slimming-20261025.json.

## DOC_SYNC_RECORD
Date: 2026-10-25
Feature: unused x64 MFC control-registration size reduction

1) AGENTS.md
- Added: reviewed x64-only registration exclusion and scoped verification contract.
- Changed: no application/firmware behavior.
- Removed: none.

2) README.md
- Added: before/after EXE size and verification limitations.
- Changed: no deployment prerequisites.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: paired startup/controls/Debug/close and actual clean preservation.
- Changed test cases: select by change impact; preserve original failed timings.
- Removed test cases: none.

Verification
- Build: scripts/build.ps1 -Configuration Release -Platform x64 -NoRun (PASS); isolated Debug build PASS.
- Runtime smoke test: owned startup/control/normal-close (PASS); hardware/visual NOT RUN.


## DOC_SYNC_RECORD
Date: 2026-10-20
Feature: compiler-file-only clean and nested delivery preservation

1) AGENTS.md
- Added: file allowlist, complete reparse preflight and automatic agent verification.
- Changed: normal clean no longer purges output folders by name.
- Removed: none.

2) README.md
- Added: canonical/legacy clean behavior and nested delivery preservation.
- Changed: no application behavior.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: help/no-deletion, 17 delivery sentinels plus source, object removal, idempotence, junction rejection and actual protected-file hashes.
- Changed test cases: clean verifies file deletion rather than folder disappearance.
- Removed test cases: none.

Verification
- Build: earlier Release x64 PASS; application source unchanged, no clean-only rebuild.
- Runtime smoke test: NOT RUN for this clean-only follow-up.
- Actual clean and expanded preservation fixtures: PASS.

## Compiler-File Clean Safety

- Both canonical/legacy clean entries use standalone `scripts/clean_intermediates.ps1`.
- Preflight all paths/reparse points before deleting allowlisted files only; never recursively purge obj/tests/verify or infer unknown content is disposable.
- Preserve EXE/INI/DLL/data/log/evidence/unknowns at every nesting level. Codex runs actual clean plus nested-delivery/help/idempotence/junction tests automatically.

## JoyStickTestTool Current Specification (as of 2026-02-15)

This document is the active implementation spec for `JoyStickTestTool`.
If old notes or old frozen specs conflict with this file, this file wins.

---

## Workspace Layout

- Follow workspace `#READ_ME_FIRST#/PROJECT_LAYOUT.md`.
- Documentation screenshots live under `docs/images`; use relative README links.
- Canonical batch aliases: `scripts/build_release_mfc.bat` and `scripts/clean_mfc.bat`.
- Existing build/clean entry points remain compatible; only known compiler intermediates are disposable, never EXE/INI/DLL/data/log/evidence/unknown files.

## 1. Scope

`JoyStickTestTool` is now an **RX-focused HID joystick monitor**.

Implemented:
- HID scan / select / connect / disconnect
- Continuous RX input report capture
- Raw 64-byte RX grid view (last 10 frames)
- Row-level `Copy` button for each RX grid row (copy row hex to clipboard)
- Descriptor-based joystick visualization (`X/Y`, `Z/Rz`, `POV`, `Buttons`)
- Right-side gamepad icon mapping panel (vector draw, no image dependency) that mirrors live control states
- Raw byte to field mapping table from HID descriptor
- Device-selection-time descriptor preview (without connect)
- Optional RX log file output

Not implemented (removed from active product scope):
- TX panel
- TX scheduler
- Handshake/Echo command flow
- TX command INI editor/load/save UI

---

## 2. Platform / Build Constraints

- OS: Windows 10/11 x64
- Language: C++17
- UI: MFC (MFC in Shared DLL = No)
- Runtime: static CRT (`/MT`)
- HID API: SetupAPI + `hid.dll`
- Third-party libraries: none
- Uses Windows built-in GDI (`msimg32.lib`) for drawing

Build scripts:
- `scripts\build_release.bat`
- `scripts\build.ps1`
- `scripts\clean_build.bat`

Build output:
- `build\JoyStickTestTool.exe`
- `build\obj\x64\Release\JoyStickTestTool.pdb`

Build behavior (`build_release.bat`):
- Stops running `JoyStickTestTool.exe` before build
- Builds `Release|x64`
- After successful build, launches `build\JoyStickTestTool.exe`

App icon resource:
- `src\res\hid.ico` is embedded as `IDI_APP_ICON` (defined in `src\JoyStickTestTool.rc`)
- This single resource drives executable icon display in File Explorer, taskbar icon, and main-window caption icon

---

## 3. Project Layout

- `src\app.*` startup app and splash
- `src\main_frame.*` main window orchestration, timers, status bar
- `src\ui\device_bar.*` device select/capability/status header
- `src\ui\rx_panel.*` RX grid, visualizer, mapping table, log controls
- `src\hid\hid_scan.*` HID enumeration
- `src\hid\hid_device.*` HID open/read/write + descriptor parsing + joystick decode
- `src\config\config_manager.*` INI persistence
- `src\log\logger.*` log file writer

---

## 4. Main Window / Layout

- Initial window size: `1024 x 768`
- Startup state: maximized
- Startup splash text: `Initializing Joystick Test Tool...`
- Deferred startup scan: main frame uses timer to scan after window show

Vertical layout:
1. DeviceBar (fixed height)
2. RX Panel (fills remaining area)
3. Status Bar

No TX panel is present.

---

## 5. DeviceBar

### 5.1 Scan and filtering

- Startup deferred scan + manual `Scan Device`
- Default filter: only joystick/gamepad
  - `UsagePage=0x0001`
  - `Usage=0x0004` (Joystick) or `0x0005` (Gamepad)
- `Show All HID` checkbox disables this filter

### 5.2 Device combo

Display format:
- `VID:PID  IN:xx OUT:xx  Manufacturer - Product`

Sort order:
- `VID -> PID -> UsagePage -> Usage -> Manufacturer -> Product -> Path`

### 5.3 Connection controls

- Single toggle button: `Connect` / `Disconnect`
- Connected state disables:
  - device combo
  - scan button
  - `Show All HID`

### 5.4 Auto reconnect

- `Auto reconnect` checkbox
- On disconnect and enabled: rescan every 1 second
- Selection priority: `PathHash` first, then `VID/PID`

### 5.5 Capability block

- `VID:PID`
- `UsagePage/Usage`
- `Reports: IN/OUT/FEAT + ReportID`
- `Copy Device Info` includes:
  - VID/PID
  - UsagePage/Usage
  - Input/Output/Feature report lengths
  - PathHash

DeviceBar rendering behavior:
- Connection-state updates are deduplicated (same state does not trigger repaint).
- Status-dot area uses partial invalidation instead of full-bar invalidation.
- DeviceBar paint uses double-buffer + erase-pass bypass to reduce flicker during high-frequency RX updates.

### 5.6 Descriptor preview without connect

When disconnected and combo selection changes, tool probes selected device descriptor and updates RX visualizer preview immediately.
This drives dynamic `Buttons 1-N` count per selected device before pressing Connect.

Preview and joystick-field rendering are limited to joystick/gamepad usage:
- `UsagePage=0x0001`
- `Usage=0x0004` or `0x0005`

When `Show All HID` is enabled and a non-joystick HID is selected, joystick button-count visualization is not driven by that descriptor.

---

## 6. HID Data / Descriptor Rules

### 6.1 RX report handling

- RX transport uses HID Input Report
- UI payload view is always 64 bytes
- If input report length is 65, first byte is treated as ReportID and skipped for payload view

### 6.2 Descriptor parsing

From HID preparsed data:
- Axes: `X(0x30)`, `Y(0x31)`, `Z(0x32)`, `Rz(0x35)`, `POV(0x39)`
- Buttons: Usage Page `0x09`, dynamic usage list

Descriptor-driven joystick decode path is applied only for joystick/gamepad usage (`UsagePage 0x0001`, `Usage 0x0004/0x0005`).

### 6.3 Dynamic button count

- Visualization button count is derived from descriptor button usages
- No fixed 12-button hardcoding for descriptor path

### 6.4 Fallback decode path

If descriptor-based state decode is unavailable for live RX payload:
- Legacy decode fallback interprets payload as:
  - Byte0 = X (int8)
  - Byte1 = Y (int8)
  - Byte2 = Button bitmask (1..8)

---

## 7. RX Panel

### 7.1 Header state text

- `RX: Idle`
- `RX: Receiving`
- `RX: Timeout`
- `RX: Idle (cleared)`

### 7.2 Visualizer area

Top visualizer sections (left to right):
- X/Y stick box
- Z/Rz bars (wider spacing between bars)
- POV circle + direction vector
- Buttons
- Right-side `Gamepad Mapping` icon panel (equal half split with left panel in normal width)

Layout notes:
- Left visualizer now has its own titled panel header (`Joystick Visualizer`) for style consistency with right-side panel.
- Left and right visualizer panels are split around the visualizer center line (approximately 50/50 width).
- Left-side four sections (`X/Y`, `Z/Rz`, `POV`, `Buttons`) use equal-width distribution across left panel content.
- Button circles remain horizontally centered inside button section
- If visual area is extremely narrow, layout falls back to left panel only.
- RX panel uses double-buffer paint + no erase-pass background clear to reduce update flicker during live joystick input
- Right icon panel stick rings are intentionally larger for clearer travel range visibility
- `SELECT/START` icon boxes use enlarged width to avoid label clipping
- In left visualizer `Z/Rz` bars, each axis value is shown above its bar and horizontally centered to that bar.
- In left visualizer `Z/Rz` bars, bar height is shortened to keep full content inside the titled panel area.

### 7.3 Right-side gamepad mapping

When right-side icon panel is visible, live HID state is mirrored on icon controls:
- POV (D-pad) also highlights icon D-pad directions
- `X/Y` (left stick) also moves left-stick icon marker
- `Z/Rz` (right stick) also moves right-stick icon marker
- Buttons also highlight icon positions:
  - Button1 = `X`
  - Button2 = `A`
  - Button3 = `B`
  - Button4 = `Y`
  - Button5 = `L1`
  - Button6 = `R1`
  - Button7 = `L2`
  - Button8 = `R2`
  - Button9 = `SELECT`
  - Button10 = `START`
  - Button11 = `Left Stick Click`
  - Button12 = `Right Stick Click`

Note:
- POV diagonal directions are shown by lighting two cardinal directions at once (for example `Up+Right` for NE); no separate diagonal button icons are used.
- If descriptor has more than 12 buttons, right icon panel still maps the first 12 controls and shows an extra pressed-count hint for remaining buttons.
- Right icon panel ABXY layout follows common Xbox-style diamond: `Y` top, `A` bottom, `X` left, `B` right.
- Right icon panel includes a checkbox `Swap DPad <-> L3` in panel top-left area.
- Default checkbox state is checked:
  - `X/Y -> icon DPad (UDLR)`
  - `POV -> icon L3 stick`
- Unchecked state restores original semantic mapping:
  - `POV -> icon DPad (UDLR)`
  - `X/Y -> icon L3 stick`

### 7.4 Raw mapping table

- `Raw Byte -> Field Mapping (payload[0..63])`
- Shows descriptor-derived field-to-bit mapping lines
- On missing descriptor info, shows fallback message

### 7.5 RX controls and grid

Order (top to bottom inside RX panel):
1. Visualizer
2. Mapping table
3. `Save to log` / `Add timestamp` / `Clear Data`
4. RX index header and 10x64 RX grid

Grid behavior:
- 10 rows, oldest at top, newest at bottom
- 64-byte columns (`0..63`)
- Each row has a `Copy` button at the row end
- `Copy` copies that row's current 64-byte hex text to clipboard
- `Copy` is enabled only for valid rows with data

Control behavior:
- `Save to log`: always enabled
- `Add timestamp`: always enabled
- `Clear Data`: enabled only when disconnected

### 7.6 Logging

- File name: `YYYYMMDD_HHMMSS_hid_rx.log`
- Location: executable directory
- RX line format:
  - without timestamp: `[RX] [..hex..]`
  - with timestamp: `[HH:MM:SS.mmm] [RX] [..hex..]`

Note: `CRxPanel` still has a TX logging helper function for compatibility hooks, but current UI flow is RX-focused.

---

## 8. Status Bar

Panes:
- `Conn: ...`
- `Err: ...`
- `RX: n`
- `JOY: ...`

Width behavior:
- Status bar pane widths are allocated to prioritize the rightmost `JOY` pane.
- Layout compresses panes from left to right limits so `JOY` text tail remains visible inside the right border.

Connection/device bar state dot:
- Disconnected
- Connected
- RX Active
- RX Timeout

---

## 9. Config Persistence

Main config file:
- `JoyStickTestTool.ini` (same folder as exe)

Actively used fields by current UI:
- last selected/connected device (`Path`, `VID`, `PID`, `PathHash`)
- RX log enable
- RX timestamp enable

Legacy compatibility fields currently still read/write in `ConfigManager`:
- scheduler/report/TX/handshake related keys

These legacy keys are retained for compatibility but are not exposed in current RX-only UI.

---

## 10. QA Reference

Use `SELF_CHECK.md` as current acceptance checklist.

---

## 11. Document Sync Template (Required)

For every feature change, update all three files in the same commit:
- `AGENTS.md` (developer spec)
- `README.md` (user manual)
- `SELF_CHECK.md` (verification checklist)

Use this fixed template block:

```md
## DOC_SYNC_RECORD
Date: YYYY-MM-DD
Feature: <short feature name>

1) AGENTS.md
- Added:
- Changed:
- Removed:

2) README.md
- Added:
- Changed:
- Removed:

3) SELF_CHECK.md
- Added test cases:
- Changed test cases:
- Removed test cases:

Verification
- Build: scripts\build_release.bat (PASS/FAIL)
- Runtime smoke test: (PASS/FAIL)
```

Minimum rule:
- If a behavior/UI/build flow changed but one of the three docs has no update, the task is incomplete.

## DOC_SYNC_RECORD
Date: 2026-02-17
Feature: right-panel icon mapping + RX flicker reduction

1) AGENTS.md
- Added: vector icon panel behavior, flicker-reduction paint notes.
- Changed: removed `example.png` image dependency/GDI+ references.
- Removed: image lookup order section.

2) README.md
- Added: double-buffer repaint note in visualizer behavior.
- Changed: right-side mapping description from image to icon panel.
- Removed: `example.png` dependency mention.

3) SELF_CHECK.md
- Added test cases: `[RX-09] Visual update flicker check`.
- Changed test cases: right-side mapping checks now reference icon panel.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-03-16
Feature: right-panel `Swap DPad <-> L3` toggle

1) AGENTS.md
- Added: right-panel swap-toggle spec and default/unchecked mapping behavior.
- Changed: right-panel mapping section now documents toggle-driven DPad/L3 source switch.
- Removed: none.

2) README.md
- Added: user-facing note for new checkbox and default checked behavior.
- Changed: icon-mapping details now include checked-mode remap (`X/Y->UDLR`, `POV->L3`).
- Removed: none.

3) SELF_CHECK.md
- Added test cases: `[RX-14] Swap DPad <-> L3 toggle behavior`.
- Changed test cases: none.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-03-16
Feature: app icon glyph vertical alignment tuning

1) AGENTS.md
- Added: icon style note for upward `J` glyph vertical offset.
- Changed: none.
- Removed: none.

2) README.md
- Added: icon alignment note (`J` glyph shifted upward for readability).
- Changed: none.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: none.
- Changed test cases: `[BLD-03]` now includes `J` glyph vertical-position check.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-03-16
Feature: app icon glyph center alignment by calculation

1) AGENTS.md
- Added: none.
- Changed: icon alignment rule from manual upward offset to glyph-bbox-based centering.
- Removed: none.

2) README.md
- Added: none.
- Changed: app icon note now describes formula-based centering in icon frame.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: none.
- Changed test cases: `[BLD-03]` now checks centered placement by frame/glyph proportion.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-03-16
Feature: app icon refresh (H -> J)

1) AGENTS.md
- Added: app icon resource ownership/scope under build constraints (`src\res\hid.ico` -> `IDI_APP_ICON`).
- Changed: none.
- Removed: none.

2) README.md
- Added: app icon source note and where it appears (File Explorer / taskbar / window caption).
- Changed: none.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: `[BLD-03] App icon consistency`.
- Changed test cases: none.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-02-18
Feature: visualizer 50/50 split + equal-width left sections

1) AGENTS.md
- Added: center-based 50/50 left-right panel split rule and equal-width left section rule.
- Changed: right panel description from optional wide-only display to right-half display in normal width.
- Removed: none.

2) README.md
- Added: center split and equal-width section notes.
- Changed: right panel display behavior text.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: `[RX-13] Visualizer equal split and fill`.
- Changed test cases: none.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-02-18
Feature: left visualizer title-row alignment + Z/Rz bar-height trim

1) AGENTS.md
- Added: left visualizer title-row style rule and shortened Z/Rz bar-height rule.
- Changed: none.
- Removed: none.

2) README.md
- Added: left title-row consistency note and Z/Rz bar-height trim note.
- Changed: none.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: `[RX-12] Left visualizer title row`.
- Changed test cases: `[RX-04]` now includes Z/Rz full-containment verification.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-02-18
Feature: Z/Rz value label placement rollback (above-bar centered)

1) AGENTS.md
- Added: none.
- Changed: Z/Rz value-label rule from in-bar center to above-bar centered placement.
- Removed: none.

2) README.md
- Added: none.
- Changed: Z/Rz note now states value text is above each bar and centered.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: none.
- Changed test cases: `[RX-04]` now checks above-bar centered value placement.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-02-17
Feature: Z/Rz bar spacing + centered value text

1) AGENTS.md
- Added: Z/Rz bar spacing and centered value text behavior in visualizer spec.
- Changed: none.
- Removed: none.

2) README.md
- Added: Z/Rz gap increase and centered in-bar value text note.
- Changed: none.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: Z/Rz spacing and centered-value verification in `[RX-04]`.
- Changed test cases: none.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-02-17
Feature: device bar flicker reduction during live joystick RX

1) AGENTS.md
- Added: DeviceBar repaint strategy (state dedupe + partial invalidate + double-buffer).
- Changed: none.
- Removed: none.

2) README.md
- Added: DeviceBar anti-flicker behavior note in UI section.
- Changed: none.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: `[DB-07] DeviceBar live-update flicker`.
- Changed test cases: none.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-02-17
Feature: stick-range retune + ABXY diamond layout

1) AGENTS.md
- Added: ABXY layout rule (`Y` top / `A` bottom / `X` left / `B` right).
- Changed: none.
- Removed: none.

2) README.md
- Added: ABXY common placement note.
- Changed: none.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: ABXY position check in `[RX-08]`.
- Changed test cases: none.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)

## DOC_SYNC_RECORD
Date: 2026-02-17
Feature: mapping icon scale tuning + status bar JOY clipping fix

1) AGENTS.md
- Added: larger stick-range ring and larger `SELECT/START` icon-box spec.
- Changed: clarified POV diagonal representation as dual-cardinal highlight.
- Removed: none.

2) README.md
- Added: notes for larger stick icon range and enlarged `SELECT/START` boxes.
- Changed: added POV diagonal representation and status-bar JOY width behavior notes.
- Removed: none.

3) SELF_CHECK.md
- Added test cases: status bar JOY right-edge clipping check.
- Changed test cases: right-side D-pad mapping now explicitly checks diagonal dual-highlight behavior.
- Removed test cases: none.

Verification
- Build: `scripts\build_release.bat` (PASS)
- Runtime smoke test: app auto-launched after build, process confirmed running (PASS)
