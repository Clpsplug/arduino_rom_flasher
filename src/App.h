//
// Created by Collapsed PLUG on 2026/07/11.
//

#ifndef ROM_FLASHER_APP_H
#define ROM_FLASHER_APP_H

namespace ecp {

struct App;

using AppRef = App&;

AppRef createApp();

void setupApp(AppRef app);

void loopApp(AppRef app);

} // namespace ecp

#endif // ROM_FLASHER_APP_H
