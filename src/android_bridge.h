#pragma once
#include "mobile_controls.h"
namespace mcu {
constexpr int AndroidCheckpointEvent = 0x4d435550;
void androidCheckpointComplete(uint32_t ticket);
struct AndroidDisplay {
    float density = 1;
    MobileInsets insets;
    bool watch = false, round = false, rotary = false;
    int thermal = 0;
};
AndroidDisplay androidDisplay();
int androidThermalStatus();
void androidChooseRom(const char *language = "en");
void androidCheckInbox(const char *language);
void androidOpenGuide(const char *language, bool mobile);
void androidPresentation(int orientation, bool keepAwake);
void androidVibrate();
} // namespace mcu
