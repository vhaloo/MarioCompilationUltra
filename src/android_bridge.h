#pragma once
#include "mobile_controls.h"
namespace mcu {
struct AndroidDisplay {
    float density = 1;
    MobileInsets insets;
};
AndroidDisplay androidDisplay();
void androidChooseRom();
void androidOpenGuide(const char *language, bool mobile);
void androidPresentation(int orientation, bool keepAwake);
void androidVibrate();
} // namespace mcu
