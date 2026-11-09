#ifndef CUSTOM_LAYOUT_HOST_TFT_H
#define CUSTOM_LAYOUT_HOST_TFT_H
#include <algorithm>
#include <assert.h>
#include <stdint.h>
#include <vector>
constexpr uint16_t ST77XX_BLACK = 0;
constexpr uint16_t ST77XX_RED = 0xf800;
// Framebuffer and write log belong only to this host capture.
class Adafruit_ST7735
{
public:
    struct Window { int x, y, width, height; };
    std::vector<uint16_t> screen = std::vector<uint16_t>(128 * 160, 0xdead);
    std::vector<Window> windows;
    int width() const { return 128; }
    int height() const { return 160; }
    void startWrite() {}
    void endWrite() {}
    void setAddrWindow(int x, int y, uint16_t w, uint16_t h)
    {
        assert(x >= 0 && y >= 0 && x + w <= 128 && y + h <= 160);
        current = {x, y, w, h};
        offset = 0;
        windows.push_back(current);
    }
    void writePixels(uint16_t *pixels, uint32_t count)
    {
        assert(offset + count <= static_cast<unsigned>(current.width * current.height));
        for (uint32_t index = 0; index < count; ++index, ++offset)
            screen[(current.y + offset / current.width) * 128 + current.x + offset % current.width] = pixels[index];
    }
    void drawRGBBitmap(int x, int y, uint16_t *pixels, uint16_t w, uint16_t h)
    {
        setAddrWindow(x, y, w, h);
        writePixels(pixels, static_cast<uint32_t>(w) * h);
    }
    void fillRect(int, int, int, int, uint16_t) {}
    void setTextColor(uint16_t, uint16_t = 0) {}
    void setTextSize(uint8_t) {}
    void write(uint8_t) {}
    void drawRect(int x, int y, int w, int h, uint16_t color)
    {
        for (int i = x; i < x + w; ++i)
        {
            screen[y * 128 + i] = color;
            screen[(y + h - 1) * 128 + i] = color;
        }
        for (int j = y; j < y + h; ++j)
        {
            screen[j * 128 + x] = color;
            screen[j * 128 + x + w - 1] = color;
        }
    }
    void setCursor(int, int) {}
    void print(const char *) {}
private:
    Window current = {};
    uint32_t offset = 0;
};
#endif
