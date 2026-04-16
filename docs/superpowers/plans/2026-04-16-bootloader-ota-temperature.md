# Bootloader OTA + Temperature Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the `Micro-Node-Bootloader` PlatformIO environment build and flash correctly on macOS, Windows, and Linux, enabling DroneCAN OTA firmware updates and MCU temperature reporting via `BatteryInfo`.

**Architecture:** Two small file changes: (1) replace the hardcoded `openocd.exe` path in `upload_bootloader_app.py` with a platform-aware lookup, and (2) add `append_crc32.py` as a pre-script in `platformio.ini` so the ArduPilot firmware descriptor (CRC + board ID) is appended on every build. Temperature is already implemented in `src/main.cpp` and needs no changes.

**Tech Stack:** PlatformIO, STM32 Arduino framework, OpenOCD (bundled with PlatformIO), Python 3 (PlatformIO scripts), ST-Link

---

### Task 1: Fix cross-platform OpenOCD path in upload script

**Files:**
- Modify: `upload_bootloader_app.py:9`

The current line 9 hardcodes `openocd.exe`, which only works on Windows. Replace it with a platform-aware lookup using `sys.platform`.

- [ ] **Step 1: Edit `upload_bootloader_app.py`**

Replace lines 1–10 (the imports and paths block) with the following. All other lines stay identical.

```python
Import("env")
import os
import subprocess
import sys

# Paths
bootloader_path = os.path.abspath(os.path.join(env.subst("$PROJECT_DIR"), "MicroNodeBootloader.bin")).replace("\\", "/")
firmware_path = os.path.abspath(os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")).replace("\\", "/")
elf_path = os.path.join(env.subst("$BUILD_DIR"), "firmware.elf")
_openocd_exe = "openocd.exe" if sys.platform == "win32" else "openocd"
openocd = os.path.join(env.subst("$PROJECT_PACKAGES_DIR"), "tool-openocd", "bin", _openocd_exe)
scripts_dir = os.path.join(env.subst("$PROJECT_PACKAGES_DIR"), "tool-openocd", "openocd", "scripts")

interface_cfg = "interface/stlink.cfg"
target_cfg = "target/stm32l4x.cfg"
```

The rest of the file (lines 16–48) is unchanged.

- [ ] **Step 2: Verify the edit is correct**

The full file should now read:

```python
Import("env")
import os
import subprocess
import sys

# Paths
bootloader_path = os.path.abspath(os.path.join(env.subst("$PROJECT_DIR"), "MicroNodeBootloader.bin")).replace("\\", "/")
firmware_path = os.path.abspath(os.path.join(env.subst("$BUILD_DIR"), "firmware.bin")).replace("\\", "/")
elf_path = os.path.join(env.subst("$BUILD_DIR"), "firmware.elf")
_openocd_exe = "openocd.exe" if sys.platform == "win32" else "openocd"
openocd = os.path.join(env.subst("$PROJECT_PACKAGES_DIR"), "tool-openocd", "bin", _openocd_exe)
scripts_dir = os.path.join(env.subst("$PROJECT_PACKAGES_DIR"), "tool-openocd", "openocd", "scripts")

interface_cfg = "interface/stlink.cfg"
target_cfg = "target/stm32l4x.cfg"

# Ensure .bin is created before upload
def generate_bin(source, target, env):
    env.Execute(f"$OBJCOPY -O binary {elf_path} {firmware_path}")

env.AddPostAction("$BUILD_DIR/${PROGNAME}.elf", generate_bin)

# Custom upload process
def custom_upload(source, target, env):
    cmd1 = [
        openocd,
        "-s", scripts_dir,
        "-f", interface_cfg,
        "-f", target_cfg,
        "-c", f'program "{bootloader_path}" 0x8000000 verify reset exit'
    ]

    cmd2 = [
        openocd,
        "-s", scripts_dir,
        "-f", interface_cfg,
        "-f", target_cfg,
        "-c", f'program "{firmware_path}" 0x800A000 verify reset exit'
    ]

    try:
        print("Flashing bootloader")
        subprocess.run(cmd1, check=True)
        print("Flashing application firmware")
        subprocess.run(cmd2, check=True)
    except subprocess.CalledProcessError as e:
        print(f"\n\n--- Upload failed ---\nException: {e}\n---------------------\n")
        env.Exit(1)

env.Replace(UPLOADCMD=custom_upload)
```

- [ ] **Step 3: Commit**

```bash
git add upload_bootloader_app.py
git commit -m "fix: cross-platform OpenOCD path in upload script"
```

---

### Task 2: Wire append_crc32.py into the bootloader build environment

**Files:**
- Modify: `platformio.ini:27`

`append_crc32.py` appends the ArduPilot firmware descriptor (CRC1, CRC2, board ID 1062) that the bootloader needs to validate and launch the app. It already hooks into `buildprog` internally — it just needs to be listed in `extra_scripts` as a `pre:` script so PlatformIO loads it.

- [ ] **Step 1: Edit `platformio.ini`**

Replace line 27:

```ini
extra_scripts = upload_bootloader_app.py
```

With:

```ini
extra_scripts =
    pre:append_crc32.py
    upload_bootloader_app.py
```

The `[env:Micro-Node-No-Bootloader]` section is unchanged — `append_crc32.py` is only needed when the bootloader is present to validate the descriptor.

- [ ] **Step 2: Build the bootloader environment and verify descriptor is appended**

```bash
pio run -e Micro-Node-Bootloader
```

Expected output includes the `append_crc32.py` descriptor lines near the end of the build:

```
Appending ArduPilot descriptor:
  Image CRC1: 0x........
  Image CRC2: 0x........
  Size:       ...... bytes
  Board ID:   1062
  Signature:  40a2e4f16468910
```

Then the build summary:

```
RAM:   [          ]   X.X% (used XXXX bytes from 65536 bytes)
Flash: [==        ]  XX.X% (used XXXXX bytes from 262144 bytes)
=== [SUCCESS] Took XX.XX seconds ===
```

If the descriptor lines do not appear, confirm that `append_crc32.py` is listed under `extra_scripts` for `[env:Micro-Node-Bootloader]` and not `[env:Micro-Node-No-Bootloader]`.

- [ ] **Step 3: Commit**

```bash
git add platformio.ini
git commit -m "feat: wire append_crc32.py into bootloader build environment"
```

---

### Task 3: Flash to hardware and verify

This task requires an ST-Link programmer connected to the BRMicroNode.

- [ ] **Step 1: Flash bootloader + application**

```bash
pio run -e Micro-Node-Bootloader --target upload
```

Expected output:
```
Flashing bootloader
...
** Programming Finished **
** Verify OK **
Flashing application firmware
...
** Programming Finished **
** Verify OK **
```

If upload fails with `openocd: command not found` or a path error, confirm PlatformIO has the `tool-openocd` package installed:

```bash
pio pkg list -g
```

Look for `tool-openocd` in the output. If missing, run:

```bash
pio pkg install -g -t tool-openocd
```

- [ ] **Step 2: Verify node appears on CAN bus**

Connect a CAN bus analyzer or open DroneCAN GUI Tool / Mission Planner. Confirm:

- Node appears with name `XRC Technologies Power Module`
- `uavcan.equipment.power.BatteryInfo` publishes at ~10 Hz
- `voltage` and `current` fields are non-zero (or zero if no load/input — check with a multimeter)
- `temperature` field is non-zero and plausible (~293–323 K, i.e. 20–50 °C in Kelvin)

- [ ] **Step 3: Verify OTA update is offered**

In DroneCAN GUI Tool, right-click the node and confirm "Update Firmware" is available (the node responds to `BeginFirmwareUpdate` requests). You do not need to complete an OTA update to verify this — confirming the option is offered and the node responds is sufficient.

- [ ] **Step 4: Commit final state if any files were changed during debugging**

```bash
git add -p
git commit -m "fix: <description of any changes made>"
```

If no files changed during this task, skip the commit.
