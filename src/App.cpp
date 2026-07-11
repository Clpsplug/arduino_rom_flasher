//
// Created by Collapsed PLUG on 2026/07/11.
//

#include "App.h"
#include <Arduino.h>
#include <ratio>

#include "IoExpanderDriver.h"
#include "SDReader.h"

using namespace ecp;

enum class AppState {
    NO_SD,
    CHECK_SD,
    READY,
    BAD_SD,
    NO_FILE_OR_AMBIGUOUS,
    WRITING,
    VERIFYING,
    ENDED
};

struct ecp::App {
    App() :
        driver(IoExpanderDriver())
        , reader(SDReader())
        , state(AppState::NO_SD)
        , romName("") {

    }

    IoExpanderDriver driver;
    SDReader reader;
    AppState state;
    const char *romName;
};

App &ecp::createApp() {
    static auto s_app = App();
    return s_app;
}


void ecp::setupApp(AppRef app) {
    Serial.begin(115200);
    app.driver.toggleAccessIndicator(false);
    app.driver.toggleSDIndicator(false);
    app.driver.toggleErrorIndicator(false);
}

void ecp::loopApp(AppRef app) {
    switch (app.state) {
        case AppState::NO_SD:
            // No SD. Indicate an error
            app.driver.toggleSDIndicator(false);
            app.driver.toggleAccessIndicator(false);
            app.driver.toggleErrorIndicator(true);
            if (app.driver.readSDCardSensor()) {
                app.state = AppState::CHECK_SD;
            }
            break;
        case AppState::CHECK_SD: {
            delay(300);
            app.driver.toggleAccessIndicator(true);
            app.driver.toggleSDIndicator(true);
            app.driver.toggleErrorIndicator(false);
            {
                auto sdInitOK = false;
                for (auto i = 0; i < 20; i++) {
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
            app.driver.toggleSDIndicator(false);
            app.driver.toggleAccessIndicator(false);
            app.driver.toggleErrorIndicator(millis() / 500 % 2 == 0);
            if (!app.driver.readSDCardSensor()) {
                app.state = AppState::NO_SD;
            }
            break;
        }
        case AppState::NO_FILE_OR_AMBIGUOUS: {
            app.driver.toggleSDIndicator(false);
            app.driver.toggleAccessIndicator(false);
            app.driver.toggleErrorIndicator(millis() / 250 % 2 == 0);
            if (!app.driver.readSDCardSensor()) {
                app.state = AppState::NO_SD;
            }
            break;
        }
        case AppState::READY: {
            Serial.println("READY");
            app.driver.toggleSDIndicator(true);
            app.driver.toggleAccessIndicator(false);
            app.driver.toggleErrorIndicator(false);
            if (!app.driver.readSDCardSensor()) {
                app.state = AppState::NO_SD;
            }
            break;
        }
        default:
            break;
    }
}
