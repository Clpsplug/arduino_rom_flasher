#include "App.h"
#include <Arduino.h>
#include <MC24FC.h>
#include <functional>
#include <map>
#include <unordered_map>

#include <lib/IoExpanderDriver.h>
#include <lib/SDReader.h>

using namespace ecp;

using AppFunc = std::function<void(AppRef)>;

constexpr int SD_DETECT_PIN = 9;
constexpr int CHIP_SELECT_PIN = 10;
constexpr int RETRY_COUNT = 5;

static void quickI2CDebug();

namespace {
bool blink(int ms, bool flip = false) {
    return millis() / ms % 2 == flip;
}
}

enum class AppState {
    INIT,
    NO_SD,
    CHECK_SD,
    READY,
    BAD_SD,
    NO_FILE_OR_AMBIGUOUS,
    WRITING,
    VERIFYING,
    ENDED,
    READ_MODE,
    ERRORED,
    EEPROM_FAULT,
    HARD_FAULT,
    MAX [[maybe_unused]]
};

struct ecp::App {
    App()
        : driver(IoExpanderDriver())
        , reader(SDReader(CHIP_SELECT_PIN))
        , eeprom(MC24FC())
        , state(AppState::INIT)
        , romName("")
        , sdPinNumber(SD_DETECT_PIN)
    {
        // Pull SD chip select pin (10) to pull-up - this is Arduino-side
        pinMode(sdPinNumber, INPUT_PULLUP);
    }

    IoExpanderDriver driver;
    SDReader reader;
    MC24FC eeprom;
    AppState state;
    const char *romName;
    int sdPinNumber;

    std::unordered_map<AppState, AppFunc> appFuncs;

    bool sdCardInserted() const {
        return digitalRead(sdPinNumber) == LOW;
    }
};

void initialization(AppRef app) {
    {
        auto fault = true;
        for (auto i = 0; i < RETRY_COUNT; i++) {
            if (app.driver.init()) {
                fault = false;
                break;
            }
        }
        if (fault) {
            app.state = AppState::HARD_FAULT;
            return;
        }
    }

    {
        auto fault = true;
        for (auto i =0; i < RETRY_COUNT; i++) {
            app.eeprom.init();
            if (app.eeprom.getError() == MC24FCError::OK) {
                fault = false;
                break;
            }
        }
        if (fault) {
            app.state = AppState::EEPROM_FAULT;
            return;
        }
    }
    if (app.driver.isWriteProtected()) {
        app.state = AppState::READ_MODE;
    } else {
        app.state = AppState::NO_SD;
    }
}

void readMode(AppRef app) {
    // If EEPROM is write-protected, this firmware falls back to read-only mode.
    app.driver.toggleStatus(blink(500), blink(500,true), false);
    if (!app.driver.isWriteProtected()) {
        app.state = AppState::INIT;
    }
}

void noSd(AppRef app) {
    // No SD. Indicate an error
    app.driver.toggleStatus(blink(500), false, true);

    if (app.sdCardInserted()) {
        app.state = AppState::CHECK_SD;
    }
    if (app.driver.isWriteProtected()) {
        app.state = AppState::INIT;
    }
}

void checkSd(AppRef app) {
    delay(300);
    app.driver.toggleStatus(false, true, false);
    {
        auto sdInitOK = false;
        for (auto i = 0; i < 5; i++) {
            if (app.reader.init()) {
                sdInitOK = true;
                break;
            }
            delay(10);
        }
        if (!sdInitOK) {
            app.state = AppState::BAD_SD;
            return;
        }
    }
    app.driver.toggleStatus(true, true, false);
    const auto romName = app.reader.getFileNameEndingWith(".ROM");
    if (app.reader.getError() != SDReaderError::OK) {
        app.state = AppState::NO_FILE_OR_AMBIGUOUS;
        return;
    }
    app.romName = romName;
    app.state = AppState::READY;
}

void badSd(AppRef app) {
    app.driver.toggleStatus(blink(500), false, blink(500));
    if (!app.sdCardInserted()) {
        app.state = AppState::NO_SD;
    }
    if (app.driver.isWriteProtected()) {
        app.state = AppState::READ_MODE;
    }
}

void ambiguousSd(AppRef app) {
    app.driver.toggleStatus(blink(250), false, blink(250));
    if (!app.sdCardInserted()) {
        app.state = AppState::NO_SD;
    }
    if (app.driver.isWriteProtected()) {
        app.state = AppState::READ_MODE;
    }
}

void flashReady(AppRef app) {
    app.driver.toggleStatus(true, false, false);
    if (!app.sdCardInserted()) {
        app.state = AppState::NO_SD;
    }
    if (app.driver.getStartSwitchDown()) {
        app.state = AppState::WRITING;
    }
    if (app.driver.isWriteProtected()) {
        app.state = AppState::READ_MODE;
    }
}

void onWrite(AppRef app) {
    app.driver.toggleStatus(true, true, false);
    app.reader.open(app.romName);
    char buf[16];
    std::uint16_t offset = 0;
    while (!app.reader.isEOF()) {
        size_t read = app.reader.read(buf, sizeof(buf));
        app.driver.toggleAccessIndicator(true);
        app.eeprom.writeAt(offset, buf, std::min(read, sizeof(buf)));
        offset += std::min(read, sizeof(buf));
        if (app.eeprom.getError() != MC24FCError::OK) {
            app.state = AppState::ERRORED;
            break;
        }
        app.driver.toggleAccessIndicator(false);
    }
    app.reader.close();
    if (app.state != AppState::ERRORED) {
        app.state = AppState::VERIFYING;
    }
}

void onVerify(AppRef app) {
    app.driver.toggleStatus(true, false, false);
    app.reader.open(app.romName);
    char buf[16];
    char eepbuf[16];
    app.eeprom.resetReadPointer();
    while (!app.reader.isEOF()) {
        size_t read = app.reader.read(buf, sizeof(buf));
        app.driver.toggleAccessIndicator(true);
        app.eeprom.readNextBytes(eepbuf, std::min(read, sizeof(eepbuf)));
        if (memcmp(buf, eepbuf, read) != 0) {
            app.state = AppState::ERRORED;
        }
        for (auto i = 0; i < 16; i++) {
            char b[32];
            sprintf(b, "e: %2d: %2d %2x s: %2d: %2d %2x", i, eepbuf[i], eepbuf[i], i, buf[i], buf[i]);
            Serial.println(b);
        }
    }
    if (app.state != AppState::ERRORED) {
        app.state = AppState::ENDED;
    }
}

void flashEnded(AppRef app) {
    app.driver.toggleStatus(blink(250), false, false);
    if (!app.sdCardInserted() && !app.driver.getStartSwitchDown()) {
        app.state = AppState::NO_SD;
    }
    if (app.driver.isWriteProtected()) {
        app.state = AppState::READ_MODE;
    }
}

void flashError(AppRef app) {
    app.driver.toggleStatus(true, false, true);
    if (!app.sdCardInserted()) {
        app.state = AppState::NO_SD;
    }
    if (app.driver.isWriteProtected()) {
        app.state = AppState::READ_MODE;
    }
}

void eepromFault(AppRef app) {
    app.driver.toggleStatus(blink(125), blink(125), blink(125));
    if (app.driver.getStartSwitchDown()) {
        app.state = AppState::INIT;
    }
}

void hardFault(AppRef app) {
    // Cannot move out of this state because the IO driver is not responding.
    // Deinit SD reader to free up pin 13, which is tied to LED.
    // We'll show error state on Arduino as much as possible.
    app.reader.deinit();
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, blink(125) ? HIGH : LOW);
}

App &ecp::createApp() {
    static App s_app = [] {
        auto a = App();
        a.appFuncs = {
            {AppState::INIT, &initialization},
            {AppState::READ_MODE, &readMode},
            {AppState::NO_SD, &noSd},
            {AppState::CHECK_SD, &checkSd},
            {AppState::READY, &flashReady},
            {AppState::BAD_SD, &badSd},
            {AppState::NO_FILE_OR_AMBIGUOUS, &ambiguousSd},
            {AppState::WRITING, &onWrite},
            {AppState::VERIFYING, &onVerify},
            {AppState::ENDED, &flashEnded},
            {AppState::ERRORED, &flashError},
            {AppState::EEPROM_FAULT, &eepromFault},
            {AppState::HARD_FAULT, &hardFault},
        };
        return a;
    }();
    return s_app;
}

void ecp::setupApp(AppRef app) {
    Serial.begin(115200);
    Serial.println("Reset");
    app.state = AppState::INIT;
}

void ecp::loopApp(AppRef app) {
    app.appFuncs[app.state](app);
    // quickI2CDebug();
}

static void quickI2CDebug() {
    Wire.beginTransmission(0x20);
    auto ret = Wire.endTransmission();
    switch (ret) {
        case 0:
            Serial.println("MCP is OK");
            break;
        case 1:
            Serial.println("Data too long for MCP");
            break;
        case 2:
            Serial.println("MCP is NOT ACKing");
            break;
        case 3:
            Serial.println("MCP NACKed during data transfer");
            break;
        case 4:
            Serial.println("Other error at MCP");
            break;
        case 5:
            Serial.println("Timed out at MCP");
            break;
        default:
            Serial.println("MCP status Unknown");
            break;
    }
    Wire.beginTransmission(0x50);
    ret = Wire.endTransmission();
    switch (ret) {
        case 0:
            Serial.println("EEPROM is OK");
            break;
        case 1:
            Serial.println("Data too long for EEPROM");
            break;
        case 2:
            Serial.println("EEPROM is NOT ACKing");
            break;
        case 3:
            Serial.println("EEPROM NACKed during data transfer");
            break;
        case 4:
            Serial.println("Other error at EEPROM");
            break;
        case 5:
            Serial.println("Timed out at EEPROM");
            break;
        default:
            Serial.println("EEPROM status Unknown");
            break;
    }
}
