#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <vector>
#include "domain/Animation.h"
#include "render/AssetManifest.h"
#include "render/AssetFormatConfig.h"

class Renderer
{
public:
    static Renderer *create(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);
    virtual ~Renderer();

    void initAnimations();
    void setAssetAppearance(const char *speciesCode, const char *outfitCode);
    bool reloadManifest();
    bool ShowSDCardImage(const char *img_path, int xmin = 0, int ymin = 0, int batch_lines = 4);
    bool ShowSDCardFrame(const char *base_path, uint16_t frame_index, int xmin = 0, int ymin = 0, int batch_lines = 4);
    bool setAnimation(AnimationId id, bool playOnce);
    bool advanceAnimationFrame();
    unsigned long frameIntervalFor(AnimationId id, unsigned long defaultIntervalMs) const;

private:
    struct AnimationState;

protected:
    Renderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);

    virtual const char *assetExtension() const = 0;
    virtual bool showImageFile(const char *imgPath,
                               int xmin,
                               int ymin,
                               int batchLines,
                               const AnimationMeta *meta) = 0;

    Adafruit_ST7735 *display() const;
    SdFat *sdCard() const;
    std::vector<uint8_t> &rowBuffer();
    std::vector<uint16_t> &lineBuffer();

private:
    Adafruit_ST7735 *tft;
    SdFat *SD;
    AnimationState *state;
};

#endif
