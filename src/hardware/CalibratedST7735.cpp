#include "hardware/CalibratedST7735.h"

CalibratedST7735::CalibratedST7735(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst)
    : Adafruit_ST7735(spiClass, cs, dc, rst)
{
}

void CalibratedST7735::setDisplayOffset(int8_t col, int8_t row)
{
    setColRowStart(col, row);
}
