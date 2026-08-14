#include "platform/hardware/CalibratedST7735.h"

CalibratedST7735::CalibratedST7735(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst)
    : Adafruit_ST7735(spiClass, cs, dc, rst)
{
}

void CalibratedST7735::setDisplayOffset(int8_t col, int8_t row)
{
    setColRowStart(col, row);
    _xstart = col;
    _ystart = row;
}

void CalibratedST7735::beginStartup(unsigned long now)
{
    begin();
    sendCommand(ST77XX_SWRESET);
    startupPhase = StartupPhase::SoftwareReset;
    startupDeadline = now + 150;
}

bool CalibratedST7735::advanceStartup(unsigned long now)
{
    if (startupPhase == StartupPhase::Ready)
        return true;
    if (startupPhase == StartupPhase::Idle || static_cast<long>(now - startupDeadline) < 0)
        return false;

    switch (startupPhase)
    {
    case StartupPhase::SoftwareReset:
        sendCommand(ST77XX_SLPOUT);
        startupPhase = StartupPhase::SleepOut;
        startupDeadline = now + 500;
        break;
    case StartupPhase::SleepOut:
        sendPanelConfiguration();
        sendCommand(ST77XX_NORON);
        startupPhase = StartupPhase::NormalOn;
        startupDeadline = now + 10;
        break;
    case StartupPhase::NormalOn:
        sendCommand(ST77XX_DISPON);
        startupPhase = StartupPhase::DisplayOn;
        startupDeadline = now + 100;
        break;
    case StartupPhase::DisplayOn:
        startupPhase = StartupPhase::Ready;
        break;
    case StartupPhase::Idle:
    case StartupPhase::Ready:
        break;
    }

    return startupPhase == StartupPhase::Ready;
}

bool CalibratedST7735::startupReady() const
{
    return startupPhase == StartupPhase::Ready;
}

bool CalibratedST7735::startupWorkWindowOpen() const
{
    return startupPhase == StartupPhase::SleepOut;
}

void CalibratedST7735::sendPanelConfiguration()
{
    static const uint8_t frameControl[] = {0x01, 0x2C, 0x2D};
    static const uint8_t partialFrameControl[] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    static const uint8_t powerControl1[] = {0xA2, 0x02, 0x84};
    static const uint8_t powerControl3[] = {0x0A, 0x00};
    static const uint8_t powerControl4[] = {0x8A, 0x2A};
    static const uint8_t powerControl5[] = {0x8A, 0xEE};
    static const uint8_t columnRange[] = {0x00, 0x00, 0x00, 0x7F};
    static const uint8_t rowRange[] = {0x00, 0x00, 0x00, 0x9F};
    static const uint8_t positiveGamma[] = {
        0x02, 0x1C, 0x07, 0x12, 0x37, 0x32, 0x29, 0x2D,
        0x29, 0x25, 0x2B, 0x39, 0x00, 0x01, 0x03, 0x10};
    static const uint8_t negativeGamma[] = {
        0x03, 0x1D, 0x07, 0x06, 0x2E, 0x2C, 0x29, 0x2D,
        0x2E, 0x2E, 0x37, 0x3F, 0x00, 0x00, 0x02, 0x10};
    static const uint8_t inversionControl = 0x07;
    static const uint8_t powerControl2 = 0xC5;
    static const uint8_t vcomControl = 0x0E;
    static const uint8_t memoryAccess = 0xC0;
    static const uint8_t colorMode = 0x05;

    sendCommand(ST7735_FRMCTR1, frameControl, sizeof(frameControl));
    sendCommand(ST7735_FRMCTR2, frameControl, sizeof(frameControl));
    sendCommand(ST7735_FRMCTR3, partialFrameControl, sizeof(partialFrameControl));
    sendCommand(ST7735_INVCTR, &inversionControl, 1);
    sendCommand(ST7735_PWCTR1, powerControl1, sizeof(powerControl1));
    sendCommand(ST7735_PWCTR2, &powerControl2, 1);
    sendCommand(ST7735_PWCTR3, powerControl3, sizeof(powerControl3));
    sendCommand(ST7735_PWCTR4, powerControl4, sizeof(powerControl4));
    sendCommand(ST7735_PWCTR5, powerControl5, sizeof(powerControl5));
    sendCommand(ST7735_VMCTR1, &vcomControl, 1);
    sendCommand(ST77XX_INVOFF);
    sendCommand(ST77XX_MADCTL, &memoryAccess, 1);
    sendCommand(ST77XX_COLMOD, &colorMode, 1);
    sendCommand(ST77XX_CASET, columnRange, sizeof(columnRange));
    sendCommand(ST77XX_RASET, rowRange, sizeof(rowRange));
    sendCommand(ST7735_GMCTRP1, positiveGamma, sizeof(positiveGamma));
    sendCommand(ST7735_GMCTRN1, negativeGamma, sizeof(negativeGamma));

    _width = ST7735_TFTWIDTH_128;
    _height = ST7735_TFTHEIGHT_160;
    rotation = 0;
    _colstart = 0;
    _rowstart = 0;
    _xstart = 0;
    _ystart = 0;
}
