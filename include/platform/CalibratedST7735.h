#ifndef CALIBRATED_ST7735_H
#define CALIBRATED_ST7735_H

#include <Adafruit_ST7735.h>

class CalibratedST7735 : public Adafruit_ST7735
{
public:
    CalibratedST7735(SPIClass *spiClass, int8_t cs, int8_t dc, int8_t rst);

    void setDisplayOffset(int8_t col, int8_t row);
    void beginStartup(unsigned long now);
    bool advanceStartup(unsigned long now);
    bool startupWorkWindowOpen() const;
    bool startupReady() const;

private:
    enum class StartupPhase : uint8_t
    {
        Idle,
        SoftwareReset,
        SleepOut,
        NormalOn,
        DisplayOn,
        Ready
    };

    StartupPhase startupPhase = StartupPhase::Idle;
    unsigned long startupDeadline = 0;

    void sendPanelConfiguration();
};

#endif
