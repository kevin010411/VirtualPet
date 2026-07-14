#ifndef TFT_DEBUG_DISPLAY_H
#define TFT_DEBUG_DISPLAY_H

#include "shared/config/AppProfile.h"

#if ENABLE_DEBUG
#include <Adafruit_ST7735.h>
#include "shared/debug/DebugDisplay.h"

class TftDebugDisplay : public DebugDisplay
{
public:
    explicit TftDebugDisplay(Adafruit_ST7735 *display);

    void showMessage(const char *title, const char *detail) override;
    void render();

private:
    Adafruit_ST7735 *tft;
    char currentTitle[21] = {};
    char currentDetail[21] = {};
    bool active = false;

    void printCentered(const char *text, int16_t y, uint16_t color);
};
#endif

#endif // TFT_DEBUG_DISPLAY_H
