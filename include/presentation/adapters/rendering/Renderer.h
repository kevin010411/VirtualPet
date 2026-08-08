#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include "animation/domain/Animation.h"
#include "shared/assets/AssetManifest.h"
#include "shared/config/AppProfile.h"
#if ENABLE_DEBUG
#include "presentation/adapters/rendering/TftDebugDisplay.h"
#endif

class Renderer
{
public:
    Renderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);
    ~Renderer();

    void initAnimations();
    void setAssetAppearance(const char *speciesCode, const char *outfitCode);
    bool reloadManifest();
    bool ShowSDCardFrame(const char *base_path, uint16_t frame_index, int xmin = 0, int ymin = 0, int batch_lines = 12);
    bool ShowAnimationFrame(AnimationId id, uint16_t frame_index, int xmin = 0, int ymin = 32, int batch_lines = 4);
    bool ShowNamedAnimationFrame(const char *name, uint16_t frame_index, int xmin = 0, int ymin = 32, int batch_lines = 12);
    bool setAnimation(AnimationId id, bool playOnce);
    bool setNamedAnimation(const char *name, bool playOnce);
    bool willRestartAnimationLoop() const;
    bool advanceAnimationFrame();
    void showInitPetNotExist();
    void showPetBehaviorLoadingError();
    void showResourceError();
    void showStatusNotFound();
#if ENABLE_DEBUG
    DebugDisplay &debugDisplay();
    void renderDebugOverlay();
#endif
    uint16_t frameCountFor(AnimationId id) const;
    uint16_t frameCountForName(const char *name) const;
    uint8_t variantCountFor(const char *baseName) const;
    const char *variantNameFor(const char *baseName, uint8_t index) const;
    unsigned long frameIntervalFor(AnimationId id, unsigned long defaultIntervalMs) const;
    unsigned long frameIntervalForName(const char *name, unsigned long defaultIntervalMs) const;
    SdFat *sdCard() const;

private:
    struct AnimationState;

    const char *assetExtension() const;
    bool showImageFile(const char *imgPath,
                       int xmin,
                       int ymin,
                       int batchLines,
                       const AnimationMeta *meta);
    Adafruit_ST7735 *display() const;
    uint8_t *readBuffer();
    size_t readBufferSize() const;
    uint16_t *lineBuffer();
    size_t lineBufferPixels() const;

    Adafruit_ST7735 *tft;
    SdFat *SD;
    AnimationState *state;
#if ENABLE_DEBUG
    TftDebugDisplay debug;
#endif
};

#endif
