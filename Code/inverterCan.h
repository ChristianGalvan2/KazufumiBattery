#ifndef INVERTER_CAN_H
#define INVERTER_CAN_H

#include <Arduino.h>
#include "driver/twai.h"

class InverterCAN {
  public:
    bool begin();
    void listen();
    uint32_t makeId(uint8_t type, uint8_t dest, uint8_t src, uint8_t func);
    void send(uint32_t id, const uint8_t *data, uint8_t len);

    // ---- High-level inverter control ----
    void powerOn(uint8_t addr);
    void powerOff(uint8_t addr);
    void powerReset(uint8_t addr);
    void setSN(uint8_t addr, uint16_t sn);
    void setFrequency(uint8_t addr, uint8_t freq50or60);
    void setACVoltageType(uint8_t addr, uint8_t type220or110);
    void setACProt(uint8_t addr, uint16_t over, uint16_t under);
    void setACAlarm(uint8_t addr, uint16_t over, uint16_t under);
    void setChargeParams(uint8_t addr, uint16_t volt, uint16_t curr);
    void setDischargeParams(uint8_t addr, uint16_t volt, uint16_t curr);
    void setDCProt(uint8_t addr, uint16_t over, uint16_t under);
    void setDCAlarm(uint8_t addr, uint16_t over, uint16_t under);
    void inquire(uint8_t func, uint8_t addr);

    // ---- Status accessors ----
    bool hasFault() const { return faultActive; }
    String getLastFaultText() const { return lastFaultText; }
    String getCurrentMode() const { return currentMode; }
    static String lastError;   // <-- add this
    static String lastMode;    // <-- add this

  private:
    bool faultActive = false;
    String lastFaultText = "";
    String currentMode = "Unknown";

    void setFault(const String &msg) { faultActive = true; lastFaultText = msg; }
    void clearFault() { faultActive = false; lastFaultText = ""; }
    void setMode(const String &mode) { currentMode = mode; }

    void sendPowerCmd(uint8_t addr, uint8_t cmd);
};

#endif
