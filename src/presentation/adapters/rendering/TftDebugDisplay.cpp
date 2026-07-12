#include "presentation/adapters/rendering/TftDebugDisplay.h"

#include <string.h>

namespace
{
constexpr int16_t kScreenWidth = 128;
constexpr int16_t kPanelX = 0;
constexpr int16_t kPanelY = 56;
constexpr int16_t kPanelWidth = 128;
constexpr int16_t kPanelHeight = 48;
constexpr uint8_t kMaxVisibleChars = 20;
} // namespace

TftDebugDisplay::TftDebugDisplay(Adafruit_ST7735 *display)
    : tft(display)
{
}

void TftDebugDisplay::showMessage(const char *title, const char *detail)
{
    if (tft == nullptr)
        return;

    strncpy(currentTitle, title == nullptr ? "" : title, sizeof(currentTitle) - 1);
    currentTitle[sizeof(currentTitle) - 1] = '\0';
    strncpy(currentDetail, detail == nullptr ? "" : detail, sizeof(currentDetail) - 1);
    currentDetail[sizeof(currentDetail) - 1] = '\0';
    active = true;
    render();
}

void TftDebugDisplay::render()
{
    if (tft == nullptr || !active)
        return;

    tft->fillRect(kPanelX, kPanelY, kPanelWidth, kPanelHeight, ST77XX_BLACK);
    tft->drawRect(kPanelX, kPanelY, kPanelWidth, kPanelHeight, ST77XX_RED);
    tft->setTextSize(1);
    tft->setTextWrap(false);
    printCentered(currentTitle, kPanelY + 12, ST77XX_RED);
    printCentered(currentDetail, kPanelY + 28, ST77XX_WHITE);
}

void TftDebugDisplay::printCentered(const char *text, int16_t y, uint16_t color)
{
    if (text == nullptr)
        text = "";

    char buffer[kMaxVisibleChars + 1] = {};
    strncpy(buffer, text, kMaxVisibleChars);
    buffer[kMaxVisibleChars] = '\0';

    const int16_t textWidth = static_cast<int16_t>(strlen(buffer) * 6);
    const int16_t x = (kScreenWidth - textWidth) / 2;
    tft->setTextColor(color, ST77XX_BLACK);
    tft->setCursor(x < 0 ? 0 : x, y);
    tft->print(buffer);
}
