#ifndef RLE_RENDERER_H
#define RLE_RENDERER_H

#include "render/Renderer.h"

#if ENABLE_SD_RLE_ASSETS
class RLERenderer : public Renderer
{
public:
    RLERenderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);

protected:
    const char *assetExtension() const override;
    bool showImageFile(const char *imgPath,
                       int xmin,
                       int ymin,
                       int batchLines,
                       const AnimationMeta *meta) override;
};
#endif

#endif
