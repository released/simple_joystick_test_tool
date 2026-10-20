# HANDOFF.md
Last updated: 2026-02-15

## Layout Audit 2026-10-20

- Screenshot moved to `docs/images/JoyStickTestTool.jpg` with no overwrite and SHA-256 verification; README uses a relative link instead of an old repository URL.
- Added compatible `scripts/build_release_mfc.bat` and `scripts/clean_mfc.bat` aliases. Original entries and application behavior remain unchanged.
- PASS: `scripts/build.ps1 -Configuration Release -Platform x64 -NoRun`; Release x64, 0 warnings/errors. No user instance was force-closed and no device was connected for the audit.
- PASS: actual `scripts/clean_mfc.bat`; 1 existing protected build files unchanged by SHA-256.
- PASS: isolated clean sentinels/idempotence (9 protected build files plus source).
- NOT RUN: fresh UI/device/parser functional acceptance; this is a directory/script-alias change, not a claim that every product checklist passed.
- Commit/push status is recorded in workspace `#READ_ME_FIRST#/PROJECT_LAYOUT_MIGRATION.md`.

## Active Workspaces
- MCU firmware: `d:\SourceCode\_Avery_NUC1261\NUC1261BSP_USB_HID_Joystick`
- PC tool: `d:\SourceCode\python\JoyStickTestTool`

---

## Current Objective
Use `JoyStickTestTool` to validate NUC1261 USB HID joystick/device behavior end-to-end.

---

## PC Tool Status (`JoyStickTestTool`)
- Project rename completed: `HidTestTool` -> `JoyStickTestTool`.
- Unused modules removed from project and filesystem:
  - `src/ui/tx_panel.*`
  - `src/core/scheduler.*`
  - `src/core/handshake_fsm.*`
  - `src/core/command_parser.*`
  - `src/config/command_config_manager.*`
- Solution/project/resource renamed:
  - `JoyStickTestTool.sln`
  - `JoyStickTestTool.vcxproj`
  - `src/JoyStickTestTool.rc`
- Build scripts updated:
  - `scripts/build.ps1` builds `JoyStickTestTool.sln` and now auto-launches exe by default.
  - Use `-NoRun` to build without launch.
  - `scripts/build_release.bat` still builds and auto-launches.
- Build output:
  - `build/JoyStickTestTool.exe`

---

## MCU Status (`NUC1261BSP_USB_HID_Joystick`)

### Descriptor / Endpoint profile
- `SampleCode/Template/descriptors.c`
  - Active report descriptor: `HID_JoystickReportDescriptor`.
- `SampleCode/Template/hid_transfer.h`
  - Current constants remain `EP2_MAX_PKT_SIZE = 64`, `EP3_MAX_PKT_SIZE = 64`.

### Transfer architecture
- `SampleCode/Template/hid_transfer.c`
  - BSP file body retained, with `#if 1 / #else` switch blocks for new API routing.
  - Includes `hid_tool_api.h`.
  - USB reset path calls `HidTool_ResetState()`.
  - `EP3_Handler()` currently disabled path (`#if 1`: joystick does not use OUT endpoint).
  - `HID_Init()` current active path skips EP3 configuration and calls `HidTool_ResetState()`.
  - `HID_SetInReport()` routes to API (`HidTool_SetInReport()`).

- `SampleCode/Template/hid_tool_api.c`
  - Joystick report generation is isolated here.
  - Maintains X/Y/button state and sends 3-byte report.
  - Debug print added for tool-side correlation:
    - `[JOY][TX] seq=... X=... Y=... BTN=... RAW=.. .. ..`

- `SampleCode/Template/main.c`
  - `HidTool_Process()` is called in `Task_100ms_Callback()` (timer-based, not loop-based).

### Build verification
- Keil project: `SampleCode/Template/Keil/Template.uvproj`
- Latest known result: `0 Error(s), 0 Warning(s)`.
- Build command:
```powershell
& 'C:\Keil_v5\UV4\UV4.exe' -b 'SampleCode\Template\Keil\Template.uvproj' -j0
```

---

## Open Decisions / Next Actions
1. Confirm final MCU profile:
   - Keep hybrid compatibility path (current), or
   - Move to strict pure joystick profile.
2. If strict joystick is chosen:
   - Decide endpoint packet size policy (`EP2`: 8 vs 64).
   - Remove/disable remaining EP3-related constants/branches consistently.
3. Run final cross-check with tool:
   - Compare tool RX with MCU `[JOY][TX]` UART logs by sequence and payload bytes.

---

## Quick Start for Next Chat
1. Open both workspaces listed above.
2. Rebuild tool (`scripts/build.ps1`) and MCU (Keil command above).
3. Plug device, run tool, verify payload correlation with MCU UART logs.
4. If behavior target is pure joystick only, perform the endpoint-size cleanup pass.
