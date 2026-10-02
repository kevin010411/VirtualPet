#ifndef LAYOUT_MEDIA_HOST_DISPLAY_H
#define LAYOUT_MEDIA_HOST_DISPLAY_H

#include <stdint.h>
#include <vector>

constexpr uint16_t ST77XX_BLACK = 0;
constexpr uint16_t ST77XX_RED = 0xF800;

// Host-only capture; no framebuffer or other storage is added to firmware.
class Adafruit_ST7735
{
public:
    std::vector<uint16_t> pixels;
    int x = 0;
    int y = 0;
    int windowWidth = 0;
    int windowHeight = 0;
    int width() const { return 128; }
    int height() const { return 160; }
    void fillRect(int, int, int, int, uint16_t) {}
    void setTextColor(uint16_t, uint16_t) {}
    void setCursor(int, int) {}
    void print(const char *) {}
    void startWrite() {}
    void endWrite() {}
    void setAddrWindow(int left, int top, uint16_t w, uint16_t h)
    {
        x = left; y = top; windowWidth = w; windowHeight = h;
    }
    void writePixels(uint16_t *data, uint32_t count)
    {
        pixels.insert(pixels.end(), data, data + count);
    }
    void drawRGBBitmap(int, int, uint16_t *data, uint16_t w, uint16_t h)
    {
        pixels.insert(pixels.end(), data, data + static_cast<uint32_t>(w) * h);
    }
};

#endif
