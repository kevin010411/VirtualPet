#pragma once
#include <stdint.h>

class PowerController
{
public:
    void interact(uint32_t now) { lastInteraction = now; }
    uint8_t brightness(uint32_t now, uint8_t voltageCap) const
    {
        const uint8_t idleCap = now - lastInteraction >= 30000U ? 64 : 255;
        return idleCap < voltageCap ? idleCap : voltageCap;
    }
    bool shouldSleep(uint32_t now, bool blocked) const
    {
        return !blocked && now - lastInteraction >= 120000U;
    }
private:
    uint32_t lastInteraction = 0;
};
