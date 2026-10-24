JoyStickTestTool Self-Check Checklist (CURRENT)

This checklist is for the current RX-focused JoyStickTestTool.
Select cases by change impact; do not repeat unrelated full suites by default.
Report actual PASS / FAIL / NOT RUN and reasons, including reused unchanged evidence.

Document sync policy:
- Every feature change must update AGENTS.md, README.md, and this file.
- Use DOC_SYNC_RECORD template defined in AGENTS.md section 11. Document Sync Template (Required).
## Verified x64 EXE slimming 2026-10-25

- PASS comparable Release baseline 2816512 bytes (original 2816512),
  formal 745984; SHA 8556E7635796FCCE52EEAD8C40EF76D088D26C04402CA2AEE5FF9D99AD6FDB47.
- PASS scripts/build.ps1 -Configuration Release -Platform x64 -NoRun; isolated Debug with version prebuild disabled.
  No compiler/linker warnings/errors reported.
- PASS hash-matched fresh own-process fixtures: identical child-control class counts,
  Debug without assertion and normal exit 0. No real INIs,
  Connect, report writes, flash or clipboard actions. Passive Windows device discovery
  may occur as in normal disconnected startup; it is not a hardware I/O validation.
- Original five-pair timing failure is retained in the JSON evidence. One fixed
  15-pair confirmation used the unchanged predeclared calibration formula and passed.
- PASS existing static runtime, compiler optimization/security, x64 PE 0x8160;
  unused VisualManager RTTI absent. No app/SDK/protocol/firmware source changes.
- PASS scoped timing: two warmups, baseline calibration before Release candidate,
  15 alternating AB/BA pairs without concurrent compilation. Delta max(floor,
  10% baseline median, 3*MAD); memory 5%; floors startup25/UI2/CPU16ms, memory1MiB.
  - StartupMs: 1411.9969 -> 1441.6994; limit 1710.9969.
  - PeakBytes: 20656128.0000 -> 20197376.0000; limit 21704704.0000.
  - CpuMs: 890.6250 -> 875.0000; limit 1031.6250.
  - WorkflowMs: 1.2224 -> 1.2098; limit 3.2224.
- Raw numeric samples/limits/original failed timing: docs/verification/exe-slimming-20261025.json.
- PASS actual clean_mfc.bat: 28 compiler files removed, 1 protected actual files
  SHA unchanged; 17 nested fixture files/source, help/idempotence PASS. Earlier
  unchanged junction/firmware/core evidence reused; private root INI hashes preserved.
- NOT RUN: physical-device/protocol throughput, unrelated firmware, visual/DPI,
  clean-PC, Win32. This scoped MFC change does not require a full hardware suite.

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

## Clean Safety Follow-up 2026-10-20

- Replaced named-directory purges with a standalone compiler-file allowlist; canonical and legacy entries share the same helper and support non-destructive help.
- PASS: shared verifier `-CleanBatch scripts/clean_mfc.bat -CleanActualProject -CheckNestedDelivery -CheckReparseRejection`: 17 fixture build sentinels plus source, help/no-deletion, object removal, repeat-clean and junction rejection before deletion.
- PASS: 1 actual protected build files unchanged (SHA-256).
- Application source/project resources are unchanged from verified baseline `8ed81cb644e718ca2a273d77e9ab410f7cbbd0ec`. The Release x64 build already passed earlier in this migration; it was not rebuilt or version-bumped just for this clean-only follow-up. This is not a new UI/hardware acceptance claim.
- NOT RUN: new functional/device/full visual/DPI/Debug acceptance; no hardware writes or user-instance closure.
- Final commit/publication evidence: workspace `#READ_ME_FIRST#/PROJECT_LAYOUT_MIGRATION.md`.

## Layout Audit 2026-10-20

- Screenshot moved to `docs/images/JoyStickTestTool.jpg` with no overwrite and SHA-256 verification; README uses a relative link instead of an old repository URL.
- Added compatible `scripts/build_release_mfc.bat` and `scripts/clean_mfc.bat` aliases. Original entries and application behavior remain unchanged.
- PASS: `scripts/build.ps1 -Configuration Release -Platform x64 -NoRun`; Release x64, 0 warnings/errors. No user instance was force-closed and no device was connected for the audit.
- PASS: actual `scripts/clean_mfc.bat`; 1 existing protected build files unchanged by SHA-256.
- PASS: isolated clean sentinels/idempotence (9 protected build files plus source).
- NOT RUN: fresh UI/device/parser functional acceptance; this is a directory/script-alias change, not a claim that every product checklist passed.
- Commit/push status is recorded in workspace `#READ_ME_FIRST#/PROJECT_LAYOUT_MIGRATION.md`.

==================================================
Section 0 — Build and Startup
==================================================

[BLD-01] Release build script
- Run: `scripts\\build_release.bat`
- Verify build succeeds (0 errors).
- Verify `build\\JoyStickTestTool.exe` is generated.
- Verify tool auto-launches after build success.

Result: PASS / FAIL

[BLD-02] Startup UX
- Launch app normally.
- Verify startup splash appears first.
- Verify main window starts maximized.
- Verify initial scan is deferred and happens shortly after main window is visible.

Result: PASS / FAIL

[BLD-03] App icon consistency
- Verify `build\\JoyStickTestTool.exe` shows the expected app icon in File Explorer.
- Launch tool and verify taskbar icon matches executable icon.
- Verify main window top-left caption icon matches executable icon.
- Verify `J` glyph appears centered in icon frame (not visually bottom-heavy or top-heavy).

Result: PASS / FAIL

==================================================
Section 1 — DeviceBar Basics
==================================================

[DB-01] Initial disconnected state
- Device combo enabled.
- `Scan Device` enabled.
- Connect toggle text is `Connect`.
- `Auto reconnect` unchecked by default.
- `Show All HID` available.
- Status indicator text shows disconnected state.

Result: PASS / FAIL

[DB-02] Device list filter (default)
- Scan with `Show All HID` unchecked.
- Verify list contains joystick/gamepad class devices only (UsagePage 0x0001, Usage 0x0004/0x0005).

Result: PASS / FAIL

[DB-03] Device list filter (show all)
- Check `Show All HID` and rescan.
- Verify list expands to broader HID set.

Result: PASS / FAIL

[DB-04] Label format and sorting
- Verify label format: `VID:PID  IN:xx OUT:xx  Manufacturer - Product`.
- Verify sorting order:
  - VID -> PID -> UsagePage -> Usage -> Manufacturer -> Product -> Path

Result: PASS / FAIL

[DB-05] Capability block content
- Verify capability rows show:
  - VID:PID
  - UsagePage/Usage
  - Reports IN/OUT/FEAT + ReportID
- Verify `VID:PID` row highlight remains readable.

Result: PASS / FAIL

[DB-06] Copy Device Info
- Click `Copy Device Info`.
- Verify clipboard includes:
  - VID:PID
  - UsagePage/Usage
  - InputReport / OutputReport / FeatureReport
  - PathHash

Result: PASS / FAIL

==================================================
Section 2 — Descriptor Preview Without Connect
==================================================

[PRE-01] Selection-change preview trigger
- Keep tool disconnected.
- Change combo selection.
- Verify RX visualizer updates to selected device descriptor preview.

Result: PASS / FAIL

[PRE-02] Dynamic button count preview
- With multiple joystick/gamepad devices selected one-by-one:
- Verify `Buttons 1-N` changes according to selected device descriptor.

Result: PASS / FAIL

[PRE-03] Mapping table preview
- While disconnected, change selected device.
- Verify `Raw Byte -> Field Mapping` lines change with selected descriptor.

Result: PASS / FAIL

[PRE-04] No button usage descriptor case
- For descriptor without button usage page, verify button section shows no-button message.

Result: PASS / FAIL

[PRE-05] Show-all non-joystick behavior
- Enable `Show All HID`.
- Select a non-joystick HID device.
- Verify joystick descriptor preview is not applied for button-count rendering.
- Verify tool does not treat non-joystick device as joystick/gamepad visual mapping source.

Result: PASS / FAIL

==================================================
Section 3 — Connect / Disconnect / Reconnect
==================================================

[CON-01] Connect success state
- Click `Connect` on selected device.
- Verify button text changes to `Disconnect`.
- Verify device combo, scan, and show-all controls become disabled.
- Verify status becomes connected / RX active.

Result: PASS / FAIL

[CON-02] Disconnect state restore
- Click `Disconnect`.
- Verify controls return to disconnected editable state.
- Verify clear-data button is enabled again.

Result: PASS / FAIL

[CON-03] Auto reconnect
- Enable `Auto reconnect`.
- Unplug and replug device.
- Verify periodic rescan (~1s) and reconnect attempt.
- Verify match priority uses PathHash first, then VID/PID.

Result: PASS / FAIL

==================================================
Section 4 — RX Capture and Visual State
==================================================

[RX-01] RX flow
- Connect and operate device.
- Verify RX counter increments.
- Verify 10x64 RX grid updates.
- Verify newest packet appears at bottom.

Result: PASS / FAIL

[RX-02] RX header state transitions
- Verify visible states:
  - `RX: Idle`
  - `RX: Receiving`
  - `RX: Timeout`
  - `RX: Idle (cleared)` after clear

Result: PASS / FAIL

[RX-03] X/Y direction consistency
- Verify visual X/Y movement direction matches current expected mapping in tool.

Result: PASS / FAIL

[RX-04] Z/Rz bars and POV
- Verify Z/Rz bars react when descriptor and live report provide values.
- Verify Z and Rz bars have increased spacing and each bar value text appears above its bar, horizontally centered.
- Verify Z/Rz bars are fully contained inside the left visualizer titled panel (no overlap/cropping).
- Verify POV direction vector reacts when POV is available.

Result: PASS / FAIL

[RX-12] Left visualizer title row
- Verify left visualizer area shows a title row (`Joystick Visualizer`).
- Verify left and right visualizer panels use consistent title-row style.

Result: PASS / FAIL

[RX-13] Visualizer equal split and fill
- Verify visualizer area is split into left/right panels around center (about half width each).
- Verify left panel 4 sections (`X/Y`, `Z/Rz`, `POV`, `Buttons`) are equal-width distributed.
- Verify no obvious large blank strip remains between/inside panels in normal window width.

Result: PASS / FAIL

[RX-05] Buttons live state
- Press/release buttons.
- Verify corresponding button circles toggle active/inactive.
- Verify displayed button index count follows descriptor-defined N.

Result: PASS / FAIL

[RX-06] Right-side gamepad panel visibility
- Resize window wide enough.
- Verify right-side `Gamepad Mapping` panel appears in visualizer area.
- Resize narrower.
- Verify panel hides gracefully and left-side visualizer remains usable.

Result: PASS / FAIL

[RX-07] Right-side dpad/stick mapping
- Operate D-pad, left stick, right stick.
- Verify left-side widgets update.
- Verify right-side gamepad icon panel also highlights corresponding D-pad / left stick / right stick locations.
- For diagonal D-pad (e.g. up-right), verify two cardinal directions are highlighted simultaneously.

Result: PASS / FAIL

[RX-14] Swap DPad <-> L3 toggle behavior
- Verify right-side `Gamepad Mapping` panel shows checkbox `Swap DPad <-> L3`.
- Verify checkbox default is checked.
- Checked behavior:
  - X/Y movement drives right-panel UDLR.
  - POV movement drives right-panel L3 stick.
- Unchecked behavior:
  - POV movement drives right-panel UDLR.
  - X/Y movement drives right-panel L3 stick.

Result: PASS / FAIL

[RX-08] Right-side button mapping
- Press buttons 1..12.
- Verify left-side button circles toggle.
- Verify right-side gamepad icon panel highlights mapped controls:
  - 1:X 2:A 3:B 4:Y
  - 5:L1 6:R1 7:L2 8:R2
  - 9:SELECT 10:START 11:L3 12:R3
- Verify ABXY icon position is diamond style:
  - `Y` top, `A` bottom, `X` left, `B` right.

Result: PASS / FAIL

[RX-09] Visual update flicker check
- Connect joystick/gamepad and continuously move sticks + D-pad + buttons for 10+ seconds.
- Verify visualizer updates without full-panel flashing/flicker.
- Verify only changed areas repaint smoothly.

Result: PASS / FAIL

[RX-10] Raw mapping panel placement
- Verify mapping panel is between visualizer and RX controls/grid.
- Verify content is readable and not overlapped.

Result: PASS / FAIL

[RX-11] SELECT/START label readability
- Verify right-side icon panel `SELECT` and `START` labels are fully readable and not clipped.

Result: PASS / FAIL

==================================================
Section 5 — RX Controls and Logging
==================================================

[CTL-01] Control enable rules
- Verify `Save to log` and `Add timestamp` are available in all states.
- Verify `Clear Data` is enabled only when disconnected.

Result: PASS / FAIL

[CTL-02] Clear Data behavior
- Click `Clear Data` while disconnected.
- Verify RX rows are cleared.
- Verify header state shows `RX: Idle (cleared)`.

Result: PASS / FAIL

[CTL-03] RX row copy button
- After RX data appears, verify each valid row shows enabled `Copy` button at row end.
- Click row `Copy` and paste content into editor.
- Verify pasted text matches that row's 64-byte hex string.
- Verify empty rows have disabled `Copy` button.

Result: PASS / FAIL

[LOG-01] Log file creation
- Check `Save to log` and receive data.
- Verify log file created in exe folder:
  - `YYYYMMDD_HHMMSS_hid_rx.log`

Result: PASS / FAIL

[LOG-02] Log line format
- With timestamp OFF: verify lines look like `[RX] [..hex..]`.
- With timestamp ON: verify lines look like `[HH:MM:SS.mmm] [RX] [..hex..]`.

Result: PASS / FAIL

==================================================
Section 6 — Status Bar
==================================================

[SB-01] Pane values
- Verify panes show:
  - `Conn: ...`
  - `Err: ...`
  - `RX: n`
  - `JOY: ...`

Result: PASS / FAIL

[SB-02] JOY content update
- On live RX, verify `JOY:` summary updates with axis/POV/button state.

Result: PASS / FAIL

[SB-03] JOY right-edge clipping
- Keep receiving live data with long JOY text (`X/Y/Z/Rz/POV/BTN...`).
- Verify rightmost characters remain visible inside status bar border (no tail clipping/covering).

Result: PASS / FAIL

[DB-07] DeviceBar live-update flicker
- Connect joystick/gamepad and continuously operate controls for 10+ seconds.
- Verify DeviceBar top area does not full-flash on each RX update.
- Verify only connection-status dot/text region repaints when state actually changes.

Result: PASS / FAIL

==================================================
Section 7 — Config Persistence
==================================================

[CFG-01] Device persistence
- Select/connect device, close app, reopen.
- Verify selection restore prefers `PathHash`, then `VID/PID`.

Result: PASS / FAIL

[CFG-02] RX options persistence
- Toggle `Save to log` and `Add timestamp`.
- Restart app.
- Verify both settings are restored.

Result: PASS / FAIL

==================================================
Section 8 — Regression Guard (Removed Features)
==================================================

[RG-01] TX UI absence
- Verify there is no TX scheduler/command panel in current UI.

Result: PASS / FAIL

[RG-02] No handshake/echo controls in UI
- Verify no handshake/echo runtime controls are shown.

Result: PASS / FAIL
