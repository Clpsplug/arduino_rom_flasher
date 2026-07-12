//
// Created by Collapsed PLUG on 2026/07/11.
//

#include "App.h"
#include <Arduino.h>
#include <MC24FC.h>

#include "IoExpanderDriver.h"
#include "SDReader.h"

using namespace ecp;

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
    MAX
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
};

App &ecp::createApp() {
    static auto s_app = App();
    return s_app;
}


void ecp::setupApp(AppRef app) {
    Serial.begin(115200);
    app.driver.toggleStatus(false, false, false);
    app.state = AppState::INIT;
}

void ecp::loopApp(AppRef app) {
    switch (app.state) {
        case AppState::INIT: {
            app.eeprom.init();
            if (app.eeprom.getError() != MC24FCError::OK) {
                app.state = AppState::HARD_FAULT;
            } else {
                app.state = AppState::NO_SD;
            }
            break;
        }
        case AppState::NO_SD: {
            // No SD. Indicate an error
            app.driver.toggleStatus(false, false, true);
            if (app.driver.readSDCardSensor()) {
                app.state = AppState::CHECK_SD;
            }
            break;
        }
        case AppState::CHECK_SD: {
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
                    break;
                }
            }
            app.driver.toggleStatus(true, true, false);
            const auto romName = app.reader.getFileNameEndingWith(".ROM");
            if (app.reader.getError() != SDReaderError::OK) {
                app.state = AppState::NO_FILE_OR_AMBIGUOUS;
                break;
            }
            app.romName = romName;
            app.state = AppState::READY;
            break;
        }
        case AppState::BAD_SD: {
            app.driver.toggleStatus(false, false, millis() / 500 % 2 == 0);
            if (!app.driver.readSDCardSensor()) {
                app.state = AppState::NO_SD;
            }
            break;
        }
        case AppState::NO_FILE_OR_AMBIGUOUS: {
            app.driver.toggleStatus(false, false, millis() / 250 % 2 == 0);
            if (!app.driver.readSDCardSensor()) {
                app.state = AppState::NO_SD;
            }
            break;
        }
        case AppState::READY: {
            app.driver.toggleStatus(true, false, false);
            if (!app.driver.readSDCardSensor()) {
                app.state = AppState::NO_SD;
            }
            if (app.driver.readSwitch()) {
                app.state = AppState::WRITING;
            }
            break;
        }
        case AppState::WRITING: {
            app.driver.toggleStatus(true, true, false);
            app.reader.open(app.romName);
            char buf[16];
            std::uint16_t offset = 0;
            while (!app.reader.isEOF()) {
                size_t read = app.reader.read(buf, sizeof(buf));
                app.driver.toggleAccessIndicator(true);
                app.eeprom.writeAt(offset, buf, std::min(read, sizeof(buf)));
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
            break;
        }
        case AppState::VERIFYING: {
            app.driver.toggleStatus(true, false, false);
            app.reader.open(app.romName);
            char buf[16];
            char eepbuf[16];
            app.eeprom.readByte(0x7fff, nullptr); // deliberate wraparound
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
            break;
        }
        case AppState::ENDED: {
            app.driver.toggleStatus(millis() / 250 % 2 == 0, false, false);
            if (!app.driver.readSDCardSensor() && !app.driver.readSwitch()) {
                app.state = AppState::NO_SD;
            }
            break;
        }
        case AppState::ERRORED: {
            app.driver.toggleStatus(true, false, true);
            if (!app.driver.readSDCardSensor()) {
                app.state = AppState::NO_SD;
            }
            break;
        }
        case AppState::HARD_FAULT: {
            bool blink = millis() / 125 % 2 == 0;
            app.driver.toggleStatus(blink, blink, blink);
            break;
        }
        default:
            break;
    }
}
