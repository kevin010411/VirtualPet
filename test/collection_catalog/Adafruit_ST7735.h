#ifndef CUSTOM_LAYOUT_HOST_TFT_H
#define CUSTOM_LAYOUT_HOST_TFT_H
#include <algorithm>
#include <assert.h>
#include <cstdlib>
#include <stdint.h>
#include <vector>
constexpr uint16_t ST77XX_BLACK = 0;
constexpr uint16_t ST77XX_RED = 0xf800;
constexpr uint16_t ST77XX_GREEN = 0x07e0;
constexpr uint16_t ST77XX_WHITE = 0xffff;
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
    void fillScreen(uint16_t color) { std::fill(screen.begin(), screen.end(), color); }
    void writeFillRect(int x, int y, int w, int h, uint16_t color)
    {
        for (int row = y; row < y + h; ++row)
            for (int col = x; col < x + w; ++col) writePixel(col, row, color);
    }
    void writePixel(int x, int y, uint16_t color)
    {
        assert(x >= 0 && y >= 0 && x < width() && y < height());
        screen[y * width() + x] = color;
    }
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
    void drawLine(int x0, int y0, int x1, int y1, uint16_t color)
    {
        const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
        if (!steps) { writePixel(x0, y0, color); return; }
        for (int step = 0; step <= steps; ++step)
            writePixel(x0 + (x1 - x0) * step / steps, y0 + (y1 - y0) * step / steps, color);
    }
    void setCursor(int, int) {}
    void print(const char *) {}
    void print(long) {}
private:
    Window current = {};
    uint32_t offset = 0;
};
#endif
