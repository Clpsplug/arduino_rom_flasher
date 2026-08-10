#include <Arduino.h>
#include "app/App.h"

void setup() {
    auto& app = ecp::createApp();
    ecp::setupApp(app);
}

void loop() {
    auto& app = ecp::createApp();
    ecp::loopApp(app);
}
