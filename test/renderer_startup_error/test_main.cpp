#include "presentation/adapters/rendering/Renderer.h"

#include <assert.h>
#include <string>

namespace
{
bool contains(const Adafruit_ST7735 &tft, const char *expected)
{
    for (const std::string &text : tft.printed)
    {
        if (text == expected)
            return true;
    }
    return false;
}
} // namespace

int main()
{
    {
        Adafruit_ST7735 tft;
        Renderer renderer(&tft, nullptr);
        renderer.showStartupResourceError("runtime.bin", "runtime manifest");

        assert(contains(tft, "runtime manifest"));
        assert(contains(tft, "runtime.bin"));
        assert(!contains(tft, "no reader detail"));
    }

    {
        Adafruit_ST7735 tft;
        Renderer renderer(&tft, nullptr);
        renderer.recordAssetDataErrorResource("species_1.data");
        renderer.showStartupResourceError("runtime.bin", "active appearance");

        assert(contains(tft, "active appearance"));
        assert(contains(tft, "species_1.data"));
        assert(!contains(tft, "no reader detail"));
    }

    {
        Adafruit_ST7735 tft;
        Renderer renderer(&tft, nullptr);
        renderer.ShowAnimationFrame({}, 0, 0);

        assert(contains(tft, "invalid asset ref"));
        assert(contains(tft, "asset reference"));
        assert(!contains(tft, "no reader detail"));
    }

    return 0;
}
