#ifndef CALIBRATED_ST7735_H
#define CALIBRATED_ST7735_H

#include <Adafruit_ST7735.h>

class CalibratedST7735 : public Adafruit_ST7735
{
public:
    CalibratedST7735(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst);

    void setDisplayOffset(int8_t col, int8_t row);
};

#endif
