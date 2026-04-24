// XRC Technologies DroneCAN Power Module

#include <Arduino.h>
#include <dronecan.h>
#include <IWatchdog.h>
#include <app.h>
#include <vector>
#include <simple_dronecanmessages.h>

// Calculations:
// 0.1mOhm resistor
// 20k-1k voltage divider
// Vout = Vbatt / 21
// 12-bit ADC (4096 bit resolution)
// INA240A2 --> 50V/V Gain
// Voltage (ADC)= I * 0.0001 * 50 = 3.3/4096
// Voltage (ADC) = 3.3/4096 = V_batt / 21
// Target current: 200A
// These are good starting numbers but actual numbers may vary
// I_batt = 0.0064453125 * V_adc
// V_batt = 0.0169189453125 * V_adc
// 1 / I_batt constant = 155.151515
// 1 / V_batt constant = 59.1053391

#define SHUNT_RESISTANCE 0.0025
#define CURRENT_PIN PA2
#define VOLTAGE_PIN PA3

float current;
float voltage;

std::vector<DroneCAN::parameter> custom_parameters = {
    {"NODEID",      UAVCAN_PROTOCOL_PARAM_VALUE_INTEGER_VALUE, 127,     0,       127},
    {"CURR_SCALE",  UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE,    1.623f,  0.1f,    1000.0f},
    {"VOLT_SCALE",  UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE,    14.705f, 0.1f,    1000.0f},
    {"CURR_OFFSET", UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE,    0.0f,    -100.0f, 100.0f},
    {"VOLT_OFFSET", UAVCAN_PROTOCOL_PARAM_VALUE_REAL_VALUE,    0.0f,    -100.0f, 100.0f},
};

DroneCAN dronecan;

uint32_t looptime = 0;

static void onTransferReceived(CanardInstance *ins, CanardRxTransfer *transfer)
{
    DroneCANonTransferReceived(dronecan, ins, transfer);
}

static bool shouldAcceptTransfer(const CanardInstance *ins,
                                 uint64_t *out_data_type_signature,
                                 uint16_t data_type_id,
                                 CanardTransferType transfer_type,
                                 uint8_t source_node_id)

{
    return false || DroneCANshoudlAcceptTransfer(ins, out_data_type_signature, data_type_id, transfer_type, source_node_id);
}

void setup()
{
    app_setup();
    IWatchdog.begin(2000000); 
    Serial.begin(115200);
    dronecan.version_major = 1;
    dronecan.version_minor = 1;
    dronecan.init(
        onTransferReceived, 
        shouldAcceptTransfer, 
        custom_parameters,
        "XRC Technologies Power Module"
    );
    pinMode(CURRENT_PIN, INPUT);
    pinMode(VOLTAGE_PIN, INPUT);

    while (true)
    {
        const uint32_t now = millis();

        // send our battery message at 10Hz
        if (now - looptime > 100)
        {
            looptime = millis();

            // construct dronecan packet
            current = analogRead(CURRENT_PIN) / dronecan.getParameter("CURR_SCALE") + dronecan.getParameter("CURR_OFFSET");
            voltage = analogRead(VOLTAGE_PIN) / dronecan.getParameter("VOLT_SCALE") + dronecan.getParameter("VOLT_OFFSET");

            // Internal ADC channels (AVREF, ATEMP) require 12-bit resolution
            // for the LL macros to compute correctly. Restore after.
            analogReadResolution(12);
            uint32_t avref_raw = analogRead(AVREF);
            uint32_t atemp_raw = analogRead(ATEMP);
            analogReadResolution(10);

            int32_t vref = (avref_raw > 0)
                ? (int32_t)__LL_ADC_CALC_VREFANALOG_VOLTAGE(avref_raw, LL_ADC_RESOLUTION_12B)
                : 3300;  // fallback to 3.3V if VREF read fails
            int32_t cpu_temp = __LL_ADC_CALC_TEMPERATURE(vref, atemp_raw, LL_ADC_RESOLUTION_12B);

            uavcan_equipment_power_BatteryInfo pkt{};
            pkt.voltage = voltage;
            pkt.current = current;
            pkt.temperature = (float)cpu_temp + 273.15f;

            sendUavcanMsg(dronecan.canard, pkt);
        }

        dronecan.cycle();
        IWatchdog.reload();
    }
}

void loop()
{
    // Doesn't work coming from bootloader ? use while loop in setup
}
