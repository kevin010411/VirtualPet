#ifndef BUNDLE_READER_ON_ACCESS_ADAFRUIT_ST7735_H
#define BUNDLE_READER_ON_ACCESS_ADAFRUIT_ST7735_H

#include <stddef.h>
#include <stdint.h>

constexpr uint16_t ST77XX_BLACK = 0;
constexpr uint16_t ST77XX_RED = 0xF800;

class Adafruit_ST7735
{
public:
    int width() const { return 128; }
    int height() const { return 160; }
    void fillRect(int, int, int, int, uint16_t) {}
    void setTextColor(uint16_t, uint16_t) {}
    void setCursor(int, int) {}
    void print(const char *) {}
    void startWrite() {}
    void setAddrWindow(int, int, uint16_t, uint16_t) {}
    void endWrite() {}
    void writePixels(uint16_t *, uint32_t) {}
    void drawRGBBitmap(int, int, uint16_t *, uint16_t, uint16_t) {}
};

#endif
