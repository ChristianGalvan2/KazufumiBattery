#include "inverterCAN.h"

#define TX_PIN GPIO_NUM_17
#define RX_PIN GPIO_NUM_16

String InverterCAN::lastError = "";
String InverterCAN::lastMode  = "";

bool InverterCAN::begin() {
    twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_PIN, RX_PIN, TWAI_MODE_NORMAL);
    g_config.tx_queue_len = 10;
    g_config.rx_queue_len = 10;
    twai_timing_config_t t_config = TWAI_TIMING_CONFIG_250KBITS();
    twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) return false;
    if (twai_start() != ESP_OK) return false;
    return true;
}

void InverterCAN::listen() {
    twai_message_t rx;
    while (twai_receive(&rx, 0) == ESP_OK) {
        uint32_t id = rx.identifier;
        uint8_t type = 0x01; //(id >> 24) & 0x1F;
        uint8_t dest = (id >> 16) & 0xFF;
        uint8_t src  = (id >> 8) & 0xFF;
        uint8_t func = id & 0xFF;

        Serial.printf("RX ID:0x%08X Type:0x%02X Dest:0x%02X Src:0x%02X Func:0x%02X DLC:%d Data:",
                      id, type, dest, src, func, rx.data_length_code);
        for (int i = 0; i < rx.data_length_code; i++) {
            Serial.printf(" %02X", rx.data[i]);
        }
        Serial.println();

        if (type == 0x01) {  // feedback frames
            switch (func) {
                case 0x51: { // AC circuit parameters 1
                    uint8_t mode = rx.data[1];
                    uint16_t Uin = rx.data[2] | (rx.data[3] << 8);
                    uint16_t Iin = rx.data[4] | (rx.data[5] << 8);
                    Serial.printf(" AC Params1: Mode=%d Uin=%.1fV Iin=%.1fA\n",
                                  mode, Uin / 10.0, Iin / 10.0);
                    break;
                }

                case 0x52: { // AC circuit parameters 2
                    uint8_t mode = rx.data[1];
                    uint16_t Ubus = rx.data[2] | (rx.data[3] << 8);
                    uint16_t Temp = rx.data[4] | (rx.data[5] << 8);
                    Serial.printf(" AC Params2: Mode=%d Ubus=%.1fV Temp=%.1fC\n",
                                  mode, Ubus / 10.0, Temp / 10.0);
                    break;
                }

                case 0x53: { // AC running state + errors
                    uint8_t mode = rx.data[1];
                    uint16_t sn = rx.data[2] | (rx.data[3] << 8);
                    uint8_t fault1 = rx.data[4];
                    uint8_t fault2 = rx.data[5];
                    uint8_t grid   = rx.data[6];
                    Serial.printf(" AC State: Mode=%d SN=0x%04X Grid=%s\n",
                                  mode, sn, grid ? "Abnormal" : "Normal");

                    if (fault1) {
                        Serial.print(" AC Fault1: ");
                        if (fault1 & (1<<0)) Serial.print("SoftStartTimeout ");
                        if (fault1 & (1<<1)) Serial.print("BUS OverVolt ");
                        if (fault1 & (1<<2)) Serial.print("BUS UnderVolt ");
                        if (fault1 & (1<<3)) Serial.print("Inverter OverVolt ");
                        if (fault1 & (1<<4)) Serial.print("Inverter UnderVolt ");
                        if (fault1 & (1<<5)) Serial.print("OverFreq ");
                        if (fault1 & (1<<6)) Serial.print("UnderFreq ");
                        if (fault1 & (1<<7)) Serial.print("GridLoss ");
                        Serial.println();
                    }
                    if (fault2) {
                        Serial.print(" AC Fault2: ");
                        if (fault2 & (1<<0)) Serial.print("TransientOC ");
                        if (fault2 & (1<<1)) Serial.print("MedialOC ");
                        if (fault2 & (1<<2)) Serial.print("Overload ");
                        if (fault2 & (1<<3)) Serial.print("ShortCircuit ");
                        if (fault2 & (1<<4)) Serial.print("FanFault ");
                        if (fault2 & (1<<5)) Serial.print("OverTemp ");
                        if (fault2 & (1<<6)) Serial.print("AddrDup ");
                        if (fault2 & (1<<7)) Serial.print("FaultOverflow ");
                        Serial.println();
                    }
                    break;
                }

                case 0x61: { // DC circuit parameters
                    uint8_t mode = rx.data[1];
                    uint16_t Udc = rx.data[2] | (rx.data[3] << 8);
                    uint16_t Idc = rx.data[4] | (rx.data[5] << 8);
                    uint16_t Ubus = rx.data[6] | (rx.data[7] << 8);
                    Serial.printf(" DC Params: Mode=%d Udc=%.1fV Idc=%.1fA Ubus=%.1fV\n",
                                  mode, Udc / 10.0, Idc / 10.0, Ubus / 10.0);
                    break;
                }

                case 0x62: { // DC running state + errors
                    uint8_t mode = rx.data[1];
                    uint16_t sn = rx.data[2] | (rx.data[3] << 8);
                    uint8_t fault1 = rx.data[4];
                    uint8_t fault2 = rx.data[5];
                    Serial.printf(" DC State: Mode=%d SN=0x%04X\n", mode, sn);

                    if (fault1) {
                        Serial.print(" DC Fault1: ");
                        if (fault1 & (1<<0)) Serial.print("SoftStartTimeout ");
                        if (fault1 & (1<<1)) Serial.print("BUS OverVolt ");
                        if (fault1 & (1<<2)) Serial.print("BUS UnderVolt ");
                        if (fault1 & (1<<3)) Serial.print("Out OverVolt ");
                        if (fault1 & (1<<4)) Serial.print("Out UnderVolt ");
                        if (fault1 & (1<<5)) Serial.print("Out OverCurr ");
                        if (fault1 & (1<<6)) Serial.print("Out Overload ");
                        if (fault1 & (1<<7)) Serial.print("OverTemp ");
                        Serial.println();
                    }
                    if (fault2) {
                        Serial.print(" DC Fault2: ");
                        if (fault2 & (1<<0)) Serial.print("FaultOverflow ");
                        if (fault2 & (1<<1)) Serial.print("MedialOC ");
                        if (fault2 & (1<<2)) Serial.print("TransientOC ");
                        if (fault2 & (1<<3)) Serial.print("SoftStartShortCircuit ");
                        if (fault2 & (1<<4)) Serial.print("AddrDup ");
                        Serial.println();
                    }
                    break;
                }

                default:
                    Serial.printf(" Unhandled feedback func: 0x%02X\n", func);
                    break;
            }
        }
    }
}

// ------------------- CAN TX -------------------

uint32_t InverterCAN::makeId(uint8_t type, uint8_t dest, uint8_t src, uint8_t func) {
    return ((uint32_t)(type & 0x1F) << 24) | ((uint32_t)dest << 16) | ((uint32_t)src << 8) | func;
}

void InverterCAN::send(uint32_t id, const uint8_t *data, uint8_t len) {
    twai_message_t msg = {};
    msg.extd = 1;
    msg.identifier = id;
    msg.data_length_code = len;
    memcpy(msg.data, data, len);
    twai_transmit(&msg, pdMS_TO_TICKS(100));
}

// ------------------- Power Control -------------------

void InverterCAN::sendPowerCmd(uint8_t addr, uint8_t cmd) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = cmd; // 0=off,1=on,2=reset
    send(makeId(0x02, 0x00, 0x0A, 0x01), d, 8);
}

void InverterCAN::powerOn(uint8_t addr) { sendPowerCmd(addr, 1); }
void InverterCAN::powerOff(uint8_t addr) { sendPowerCmd(addr, 0); }
void InverterCAN::powerReset(uint8_t addr) { sendPowerCmd(addr, 2); }

// ------------------- Settings -------------------

void InverterCAN::setSN(uint8_t addr, uint16_t sn) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = sn & 0xFF;
    d[3] = (sn >> 8) & 0xFF;
    send(makeId(0x02, 0x00, 0x0A, 0x05), d, 8);
}

void InverterCAN::setFrequency(uint8_t addr, uint8_t freq50or60) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = freq50or60; // 0=50Hz,1=60Hz
    send(makeId(0x02, 0x00, 0x0A, 0x11), d, 8);
}

void InverterCAN::setACVoltageType(uint8_t addr, uint8_t type220or110) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = type220or110; // 0=220V,1=110V
    send(makeId(0x02, 0x00, 0x0A, 0x12), d, 8);
}

void InverterCAN::setACProt(uint8_t addr, uint16_t over, uint16_t under) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = (over * 10) & 0xFF; d[3] = ((over * 10) >> 8) & 0xFF;
    d[4] = (under * 10) & 0xFF; d[5] = ((under * 10) >> 8) & 0xFF;
    send(makeId(0x02, 0x00, 0x0A, 0x13), d, 8);
}

void InverterCAN::setACAlarm(uint8_t addr, uint16_t over, uint16_t under) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = (over * 10) & 0xFF; d[3] = ((over * 10) >> 8) & 0xFF;
    d[4] = (under * 10) & 0xFF; d[5] = ((under * 10) >> 8) & 0xFF;
    send(makeId(0x02, 0x00, 0x0A, 0x14), d, 8);
}

void InverterCAN::setChargeParams(uint8_t addr, uint16_t volt, uint16_t curr) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = (volt * 10) & 0xFF; d[3] = ((volt * 10) >> 8) & 0xFF;
    d[4] = (curr * 10) & 0xFF; d[5] = ((curr * 10) >> 8) & 0xFF;
    send(makeId(0x02, 0x00, 0x0A, 0x31), d, 8);
}

void InverterCAN::setDischargeParams(uint8_t addr, uint16_t volt, uint16_t curr) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = (volt * 10) & 0xFF; d[3] = ((volt * 10) >> 8) & 0xFF;
    d[4] = (curr * 10) & 0xFF; d[5] = ((curr * 10) >> 8) & 0xFF;
    send(makeId(0x02, 0x00, 0x0A, 0x32), d, 8);
}

void InverterCAN::setDCProt(uint8_t addr, uint16_t over, uint16_t under) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = (over * 10) & 0xFF; d[3] = ((over * 10) >> 8) & 0xFF;
    d[4] = (under * 10) & 0xFF; d[5] = ((under * 10) >> 8) & 0xFF;
    send(makeId(0x02, 0x00, 0x0A, 0x33), d, 8);
}

void InverterCAN::setDCAlarm(uint8_t addr, uint16_t over, uint16_t under) {
    uint8_t d[8] = {};
    d[0] = addr;
    d[2] = (over * 10) & 0xFF; d[3] = ((over * 10) >> 8) & 0xFF;
    d[4] = (under * 10) & 0xFF; d[5] = ((under * 10) >> 8) & 0xFF;
    send(makeId(0x02, 0x00, 0x0A, 0x34), d, 8);
}

void InverterCAN::inquire(uint8_t func, uint8_t addr) {
    uint8_t d[8] = {};
    d[0] = addr; // all other bytes zero for inquiry
    send(makeId(0x03, 0x00, 0x0A, func), d, 8);
}
