# Temperature Sensing Design

**Date:** 2026-04-15  
**Status:** Approved

## Summary

Add MCU internal temperature to the DroneCAN `BatteryInfo` message already broadcast by the power module firmware.

## Scope

Single file: `src/main.cpp`. No new dependencies, libraries, hardware, or parameters.

## Implementation

Inside the existing 10Hz loop, read the STM32L431 internal temperature sensor using the LL ADC macros already available in the build environment. The AVREF channel gives a calibrated supply voltage used to correct the temperature reading.

```cpp
int32_t vref = __LL_ADC_CALC_VREFANALOG_VOLTAGE(analogRead(AVREF), LL_ADC_RESOLUTION_12B);
int32_t cpu_temp = __LL_ADC_CALC_TEMPERATURE(vref, analogRead(ATEMP), LL_ADC_RESOLUTION_12B);
pkt.temperature = cpu_temp;
```

The existing commented-out line `//pkt.temperature = INA.getTemperature();` is replaced with `pkt.temperature = cpu_temp;`.

## Decisions

- **Sensor**: Internal MCU only — no external sensor present on this hardware.
- **Message**: `BatteryInfo.temperature` field — already being sent at 10Hz, no new message type needed.
- **Rate**: 10Hz, matching the existing V/I read rate. Temperature changes slowly enough that this is not a concern.
- **No dedicated `device_Temperature` message**: Added complexity with no benefit for this use case.

## What does not change

- Loop timing and structure
- Voltage and current scaling factors
- DroneCAN parameters
- Any other files
