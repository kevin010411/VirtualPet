#ifndef RENDERER_STARTUP_ERROR_ADAFRUIT_ST7735_H
#define RENDERER_STARTUP_ERROR_ADAFRUIT_ST7735_H

#include <stdint.h>
#include <string>
#include <vector>

constexpr uint16_t ST77XX_BLACK = 0;
constexpr uint16_t ST77XX_RED = 0xF800;
constexpr uint16_t ST77XX_WHITE = 0xFFFF;

class Adafruit_ST7735
{
public:
    std::vector<std::string> printed;

    int width() const { return 128; }
    int height() const { return 160; }
    void fillRect(int, int, int, int, uint16_t) {}
    void drawRect(int, int, int, int, uint16_t) {}
    void setTextColor(uint16_t, uint16_t) {}
    void setTextSize(uint8_t) {}
    void setTextWrap(bool) {}
    void setCursor(int, int) {}
    void print(const char *text) { printed.emplace_back(text == nullptr ? "" : text); }
    void startWrite() {}
    void setAddrWindow(int, int, uint16_t, uint16_t) {}
    void endWrite() {}
    void writePixels(uint16_t *, uint32_t) {}
    void drawRGBBitmap(int, int, uint16_t *, uint16_t, uint16_t) {}
};

#endif
