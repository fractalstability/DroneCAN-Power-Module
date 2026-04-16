# Bootloader OTA + Temperature Design

**Date:** 2026-04-16

## Goal

Enable DroneCAN over-the-air firmware updates on the BRMicroNode (STM32L431) and confirm MCU temperature is published in `BatteryInfo`. Both features depend on the `Micro-Node-Bootloader` build environment being fully functional on macOS, Windows, and Linux.

---

## Context

The repo already contains everything needed conceptually:

- `MicroNodeBootloader.bin` — ArduPilot DroneCAN bootloader for BeyondRobotix M-Periph, confirmed correct for this hardware (string `org.ardupilot.BeyondRobotixM-Periph` embedded; reset vector at `0x080001B0`; 16KB, occupies first 40KB of flash)
- `ldscript.ld` — places the app at `0x800A000` (40KB offset), reserves `0x20000000–0x20000100` as the bootloader comms region
- `app.cpp` — sets `SCB->VTOR = 0x0800A000` and reinitializes clocks on startup from bootloader
- `dronecan.cpp` — handles `BeginFirmwareUpdate`: writes server node ID + firmware path to the comms struct at `0x20000000`, then calls `NVIC_SystemReset()` so the bootloader takes over
- `append_crc32.py` — appends the ArduPilot firmware descriptor (CRC1, CRC2, board ID 1062) that the bootloader requires to validate the app before launching it
- `upload_bootloader_app.py` — flashes bootloader at `0x8000000`, then app at `0x800A000` using OpenOCD via ST-Link
- `src/main.cpp` — already reads AVREF + ATEMP via `__LL_ADC_CALC_VREFANALOG_VOLTAGE` / `__LL_ADC_CALC_TEMPERATURE` and populates `pkt.temperature` in Kelvin

Two gaps prevent this from working:

1. `upload_bootloader_app.py` hardcodes `openocd.exe` — fails on macOS and Linux
2. `append_crc32.py` is not listed in `extra_scripts` for the `Micro-Node-Bootloader` env — the firmware descriptor is never appended, so the bootloader will refuse to launch the app

---

## Changes

### 1. `upload_bootloader_app.py` — cross-platform OpenOCD binary

**What:** Replace the hardcoded `openocd.exe` path with a platform-aware lookup.

```python
import sys
exe = "openocd.exe" if sys.platform == "win32" else "openocd"
openocd = os.path.join(env.subst("$PROJECT_PACKAGES_DIR"), "tool-openocd", "bin", exe)
```

`scripts_dir`, `interface_cfg`, and `target_cfg` are unchanged — PlatformIO normalizes separators internally. No other logic changes.

### 2. `platformio.ini` — wire in `append_crc32.py`

**What:** Add `append_crc32.py` as a pre-script in the `Micro-Node-Bootloader` env so the ArduPilot descriptor is appended on every build before upload runs.

```ini
extra_scripts =
    pre:append_crc32.py
    upload_bootloader_app.py
```

`append_crc32.py` already hooks into `buildprog` and needs no code changes.

### 3. Temperature — no changes

`src/main.cpp` already implements temperature correctly (committed `dec8507`). The `Micro-Node-Bootloader` build uses the same `src/main.cpp`, so temperature works as-is once the bootloader env is functional.

---

## Flash Memory Layout

| Region | Address | Size | Contents |
|---|---|---|---|
| Bootloader | `0x08000000` | 40 KB | `MicroNodeBootloader.bin` |
| Application | `0x0800A000` | 216 KB | App firmware + ArduPilot descriptor at end |
| Bootloader comms | `0x20000000` | 256 B | RAM handoff struct (node ID, firmware path) |

---

## OTA Update Flow (post-implementation)

1. GCS (Mission Planner / DroneCAN GUI) sends `BeginFirmwareUpdate` to the node
2. Node writes server node ID + filename to comms struct at `0x20000000`, sends OK response, resets
3. Bootloader reads comms struct, issues `uavcan.protocol.file.Read` requests to the GCS
4. Bootloader validates firmware descriptor (CRC1, CRC2, board ID), writes to `0x0800A000`
5. Bootloader jumps to app

---

## Out of Scope

- Building or modifying the bootloader binary itself
- Windows ST-Link driver setup (pre-existing requirement)
- OTA end-to-end verification with physical hardware (manual step after flash)
