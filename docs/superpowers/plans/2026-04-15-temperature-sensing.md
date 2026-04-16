# Temperature Sensing Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Populate the `temperature` field in the DroneCAN `BatteryInfo` message using the STM32L431 internal MCU temperature sensor.

**Architecture:** Two LL ADC macro calls read AVREF (supply voltage reference) and ATEMP (internal temperature sensor) from the STM32's built-in ADC channels. The result is assigned to `pkt.temperature` inside the existing 10Hz loop. No new files, no new dependencies.

**Tech Stack:** PlatformIO, STM32 Arduino framework, STM32 LL ADC macros (`__LL_ADC_CALC_VREFANALOG_VOLTAGE`, `__LL_ADC_CALC_TEMPERATURE`)

---

### Task 1: Add MCU temperature to BatteryInfo

**Files:**
- Modify: `src/main.cpp`

**Context:** The 10Hz loop currently reads `current` and `voltage` from analog pins PA2/PA3, then constructs a `uavcan_equipment_power_BatteryInfo` packet. The `temperature` field is left at zero (default) — there is a commented-out line `//pkt.temperature = INA.getTemperature();` that marks where it should go.

The STM32L431 has two internal ADC channels always available:
- `AVREF` — internal reference voltage channel, used to compute the actual supply voltage
- `ATEMP` — internal temperature sensor channel

The LL macros `__LL_ADC_CALC_VREFANALOG_VOLTAGE` and `__LL_ADC_CALC_TEMPERATURE` are part of the STM32 HAL/LL layer included by the Arduino STM32 framework. No extra `#include` is needed — see `examples/CPU_Temp/main.cpp` for confirmation they compile without additional headers.

- [ ] **Step 1: Add the temperature read and assign it to the packet**

Open `src/main.cpp`. Find the 10Hz block (starts at `if (now - looptime > 100)`). It currently looks like this:

```cpp
if (now - looptime > 100)
{
    looptime = millis();

    // construct dronecan packet
    current = analogRead(CURRENT_PIN) / CURRENT_SCALE_FACTOR;
    voltage = analogRead(VOLTAGE_PIN) / VOLTAGE_SCALE_FACTOR;
    uavcan_equipment_power_BatteryInfo pkt{};
    pkt.voltage = voltage;
    pkt.current = current;
    //pkt.temperature = INA.getTemperature();

    sendUavcanMsg(dronecan.canard, pkt);
}
```

Replace it with:

```cpp
if (now - looptime > 100)
{
    looptime = millis();

    // construct dronecan packet
    current = analogRead(CURRENT_PIN) / CURRENT_SCALE_FACTOR;
    voltage = analogRead(VOLTAGE_PIN) / VOLTAGE_SCALE_FACTOR;
    int32_t vref = __LL_ADC_CALC_VREFANALOG_VOLTAGE(analogRead(AVREF), LL_ADC_RESOLUTION_12B);
    int32_t cpu_temp = __LL_ADC_CALC_TEMPERATURE(vref, analogRead(ATEMP), LL_ADC_RESOLUTION_12B);
    uavcan_equipment_power_BatteryInfo pkt{};
    pkt.voltage = voltage;
    pkt.current = current;
    pkt.temperature = cpu_temp;

    sendUavcanMsg(dronecan.canard, pkt);
}
```

- [ ] **Step 2: Build and verify it compiles**

In PlatformIO, build the `Micro-Node-No-Bootloader` environment:

```bash
pio run -e Micro-Node-No-Bootloader
```

Expected output ends with:
```
RAM:   [          ]   X.X% (used XXXX bytes from 65536 bytes)
Flash: [==        ]  XX.X% (used XXXXX bytes from 262144 bytes)
=== [SUCCESS] Took XX.XX seconds ===
```

If the build fails with `'AVREF' was not declared` or `'__LL_ADC_CALC_TEMPERATURE' was not declared`, check that you are building the STM32 Arduino target (not a native/test environment). These symbols are provided by the STM32 Arduino core for STM32L4 targets.

- [ ] **Step 3: Flash to device and verify temperature appears on CAN bus**

Flash using ST-Link:

```bash
pio run -e Micro-Node-No-Bootloader --target upload
```

Connect a CAN bus analyzer or use Mission Planner / QGroundControl / DroneCAN GUI tool (e.g. `dronecan_gui_tool`). Subscribe to `uavcan.equipment.power.BatteryInfo`. Confirm:
- `temperature` field is non-zero
- Value is plausible for room temperature (roughly 20–35 °C)
- The field updates every ~100ms

Note: the STM32 internal sensor reads die temperature, which runs a few degrees above ambient. A reading of 25–45 °C at room temperature is normal.

- [ ] **Step 4: Commit**

```bash
git add src/main.cpp
git commit -m "feat: add MCU internal temperature to BatteryInfo"
```

- [ ] **Step 5: Push to private fork**

```bash
git push origin master
```
