#include "App.h"
#include <Arduino.h>
#include <MC24FC.h>
#include <functional>
#include <map>
#include <unordered_map>

#include "IoExpanderDriver.h"
#include "SDReader.h"

using namespace ecp;

using AppFunc = std::function<void(AppRef)>;

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
    ERRORED,
    HARD_FAULT,
    MAX [[maybe_unused]]
};

struct ecp::App {
    App() :
        driver(IoExpanderDriver())
        , reader(SDReader())
        , eeprom(MC24FC())
        , state(AppState::INIT)
        , romName("") {

    }

    IoExpanderDriver driver;
    SDReader reader;
    MC24FC eeprom;
    AppState state;
    const char *romName;

    std::unordered_map<AppState, AppFunc> appFuncs;
};

void initialization(AppRef app) {
    app.eeprom.init();
    if (app.eeprom.getError() != MC24FCError::OK) {
        app.state = AppState::HARD_FAULT;
    } else {
        app.state = AppState::NO_SD;
    }
}

void noSd(AppRef app) {
    // No SD. Indicate an error
    app.driver.toggleStatus(false, false, true);
    if (app.driver.readSDCardSensor()) {
        app.state = AppState::CHECK_SD;
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
    app.driver.toggleStatus(false, false, millis() / 500 % 2 == 0);
    if (!app.driver.readSDCardSensor()) {
        app.state = AppState::NO_SD;
    }
}

void ambiguousSd(AppRef app) {
    app.driver.toggleStatus(false, false, millis() / 250 % 2 == 0);
    if (!app.driver.readSDCardSensor()) {
        app.state = AppState::NO_SD;
    }
}

void flashReady(AppRef app) {
    app.driver.toggleStatus(true, false, false);
    if (!app.driver.readSDCardSensor()) {
        app.state = AppState::NO_SD;
    }
    if (app.driver.readSwitch()) {
        app.state = AppState::WRITING;
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
    app.driver.toggleStatus(millis() / 250 % 2 == 0, false, false);
    if (!app.driver.readSDCardSensor() && !app.driver.readSwitch()) {
        app.state = AppState::NO_SD;
    }
}

void flashError(AppRef app) {
    app.driver.toggleStatus(true, false, true);
    if (!app.driver.readSDCardSensor()) {
        app.state = AppState::NO_SD;
    }
}

void hardFault(AppRef app) {
    const auto blink = millis() / 125 % 2 == 0;
    app.driver.toggleStatus(blink, blink, blink);
}

App &ecp::createApp() {
    static auto s_app = App();
    s_app.appFuncs = {
        {AppState::INIT, &initialization},
        {AppState::NO_SD, &noSd},
        {AppState::CHECK_SD, &checkSd},
        {AppState::READY, &flashReady},
        {AppState::BAD_SD, &badSd},
        {AppState::NO_FILE_OR_AMBIGUOUS, &ambiguousSd},
        {AppState::WRITING, &onWrite},
        {AppState::VERIFYING, &onVerify},
        {AppState::ENDED, &flashEnded},
        {AppState::ERRORED, &flashError},
        {AppState::HARD_FAULT, &hardFault},
    };
    return s_app;
}

void ecp::setupApp(AppRef app) {
    Serial.begin(115200);
    app.driver.toggleStatus(false, false, false);
    app.state = AppState::INIT;
}

void ecp::loopApp(AppRef app) {
    app.appFuncs[app.state](app);
}
