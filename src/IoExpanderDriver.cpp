//
// Created by Collapsed PLUG on 2026/07/11.
//

#include <Arduino.h>
#include "IoExpanderDriver.h"

using namespace ecp;

constexpr int SD_INDICATOR_PIN = 0;
constexpr int ACCESS_INDICATOR_PIN = 1;
constexpr int ERROR_INDICATOR_PIN = 2;
constexpr int START_WRITE_SWITCH_PIN = 3;
constexpr int WRITE_PROTECTION_SWITCH = 4;

IoExpanderDriver::IoExpanderDriver() :
    error(IoExpanderError::OK),
    mcp(Adafruit_MCP23X17()) {
    if (!this->mcp.begin_I2C(0x20)) {
        this->error = IoExpanderError::INIT_FAIL;
        return;
    }

    this->mcp.pinMode(SD_INDICATOR_PIN, OUTPUT);
    this->mcp.pinMode(ACCESS_INDICATOR_PIN, OUTPUT);
    this->mcp.pinMode(ERROR_INDICATOR_PIN, OUTPUT);
    this->mcp.pinMode(START_WRITE_SWITCH_PIN, INPUT_PULLUP);
    this->mcp.pinMode(WRITE_PROTECTION_SWITCH, INPUT_PULLUP);
    this->mcp.digitalWrite(SD_INDICATOR_PIN, LOW);
    this->mcp.digitalWrite(ACCESS_INDICATOR_PIN, LOW);
    this->mcp.digitalWrite(ERROR_INDICATOR_PIN, LOW);
}

void IoExpanderDriver::toggleStatus(bool sd, bool access, bool error) {
    this->toggleSDIndicator(sd);
    this->toggleAccessIndicator(access);
    this->toggleErrorIndicator(error);
}

void IoExpanderDriver::toggleSDIndicator(bool on) {
    this->mcp.digitalWrite(SD_INDICATOR_PIN, on ? HIGH : LOW);
}

void IoExpanderDriver::toggleAccessIndicator(bool on) {
    this->mcp.digitalWrite(ACCESS_INDICATOR_PIN, on ? HIGH : LOW);
}

void IoExpanderDriver::toggleErrorIndicator(bool on) {
    this->mcp.digitalWrite(ERROR_INDICATOR_PIN, on ? HIGH : LOW);
}

bool IoExpanderDriver::readStartWriteSwitch() {
    return this->mcp.digitalRead(START_WRITE_SWITCH_PIN) == LOW;
}

bool IoExpanderDriver::readWriteProtectionSwitch() {
    return this->mcp.digitalRead(WRITE_PROTECTION_SWITCH) == HIGH;
}
