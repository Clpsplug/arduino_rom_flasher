//
// Created by Collapsed PLUG on 2026/07/11.
//

#include "IoExpanderDriver.h"

using namespace ecp;

constexpr int SD_INDICATOR_PIN = 7;
constexpr int ACCESS_INDICATOR_PIN = 6;
constexpr int ERROR_INDICATOR_PIN = 5;
constexpr int SWITCH_PIN = 4;
constexpr int SDCARD_SENSOR_PIN = 3;

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
    this->mcp.pinMode(SWITCH_PIN, INPUT_PULLUP);
    this->mcp.pinMode(SDCARD_SENSOR_PIN, INPUT_PULLUP);
    this->mcp.digitalWrite(SD_INDICATOR_PIN, LOW);
    this->mcp.digitalWrite(ACCESS_INDICATOR_PIN, LOW);
    this->mcp.digitalWrite(ERROR_INDICATOR_PIN, LOW);
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

bool IoExpanderDriver::readSwitch() {
    return this->mcp.digitalRead(SWITCH_PIN) == LOW;
}

bool IoExpanderDriver::readSDCardSensor() {
    return this->mcp.digitalRead(SDCARD_SENSOR_PIN) == LOW;
}
