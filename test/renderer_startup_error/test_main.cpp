#include "resources/AssetResources.h"
#include "display/Renderer.h"

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
        AssetResources resources(nullptr);
        resources.recordAssetDataErrorResource("runtime.bin");
        resources.recordAssetDataErrorResource("later error");
        assert(resources.firstAssetDataError() == AssetData::BundleError::InvalidPack);
        assert(std::string(resources.firstAssetDataErrorResource()) == "runtime.bin");
        Adafruit_ST7735 tft;
        {
            Renderer renderer(&tft, resources);
            renderer.showStartupResourceError("startup", "active appearance");
            assert(contains(tft, "runtime.bin"));
        }
        assert(std::string(resources.firstAssetDataErrorResource()) == "runtime.bin");
    }
    {
        Adafruit_ST7735 tft;
        AssetResources resources(nullptr);
        Renderer renderer(&tft, resources);
        renderer.showStartupResourceError("startup", "prepare game");

        assert(contains(tft, "prepare game"));
        assert(contains(tft, "startup"));
        assert(!contains(tft, "no reader detail"));
    }

    {
        Adafruit_ST7735 tft;
        AssetResources resources(nullptr);
        Renderer renderer(&tft, resources);
        renderer.showStartupResourceError("runtime.bin", "runtime manifest");

        assert(contains(tft, "runtime manifest"));
        assert(contains(tft, "runtime.bin"));
        assert(!contains(tft, "no reader detail"));
    }

    {
        Adafruit_ST7735 tft;
        AssetResources resources(nullptr);
        Renderer renderer(&tft, resources);
        resources.recordAssetDataErrorResource("species_1.data");
        renderer.showStartupResourceError("runtime.bin", "active appearance");

        assert(contains(tft, "active appearance"));
        assert(contains(tft, "species_1.data"));
        assert(!contains(tft, "no reader detail"));
    }

    {
        Adafruit_ST7735 tft;
        AssetResources resources(nullptr);
        Renderer renderer(&tft, resources);
        renderer.ShowAnimationFrame({}, 0, 0);

        assert(contains(tft, "invalid asset ref"));
        assert(contains(tft, "asset reference"));
        assert(!contains(tft, "no reader detail"));
    }

    return 0;
}
