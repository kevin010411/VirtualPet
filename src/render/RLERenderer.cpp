#include "render/RLERenderer.h"

#if ENABLE_SD_RLE_ASSETS
#include "render/FrameDecoder.h"

RLERenderer::RLERenderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
    : Renderer(ref_tft, ref_SD)
{
}

const char *RLERenderer::assetExtension() const
{
    return ".rle";
}

bool RLERenderer::showImageFile(const char *imgPath,
                                int xmin,
                                int ymin,
                                int batchLines,
                                const AnimationMeta *meta)
{
    (void)batchLines;
    const bool validateSize = meta != nullptr;
    const uint16_t expectedWidth = validateSize ? meta->width : 0;
    const uint16_t expectedHeight = validateSize ? meta->height : 0;
    return FrameDecoder::showRleImage(sdCard(), display(), lineBuffer(), imgPath, expectedWidth, expectedHeight, validateSize, xmin, ymin);
}
#endif
