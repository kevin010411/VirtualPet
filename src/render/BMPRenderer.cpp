#include "render/BMPRenderer.h"

#if ENABLE_SD_BMP_ASSETS
#include "render/FrameDecoder.h"

BMPRenderer::BMPRenderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
    : Renderer(ref_tft, ref_SD)
{
}

const char *BMPRenderer::assetExtension() const
{
    return ".bmp";
}

bool BMPRenderer::showImageFile(const char *imgPath,
                                int xmin,
                                int ymin,
                                int batchLines,
                                const AnimationMeta *meta)
{
    (void)meta;
    return FrameDecoder::showBmpImage(sdCard(), display(), rowBuffer(), lineBuffer(), imgPath, xmin, ymin, batchLines);
}
#endif
