#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include "shared/assets/AssetRuntimeContract.h"
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
    void setAssetAppearance(uint8_t speciesSlot, uint8_t outfitSlot);
    bool configureAssetBundle(const AssetData::BundleId &bundleId);
    bool ShowDataFrame(const AssetData::AssetFrameAddress &address,
                       int xmin = 0,
                       int ymin = 32,
                       int batch_lines = 12);
    bool ShowAnimationFrame(const AssetData::AnimationRef &animation,
                            uint8_t versionIndex,
                            uint16_t frameIndex,
                            int xmin = 0,
                            int ymin = 32,
                            int batchLines = 12);
    bool setAnimation(const AssetData::AnimationRef &animation,
                      uint8_t versionIndex,
                      bool playOnce);
    bool willRestartAnimationLoop() const;
    bool advanceAnimationFrame();
    bool animationFrameFailed() const;
    void showResourceError();
    void showResourceError(const char *resource);
    void recordAssetDataErrorResource(const char *resource);
#if ENABLE_DEBUG
    DebugDisplay &debugDisplay();
    void renderDebugOverlay();
    bool hasRenderedFrame() const;
#endif
    uint16_t frameCountFor(const AssetData::AnimationRef &animation,
                           uint8_t versionIndex = 0);
    uint8_t versionCountFor(const AssetData::AnimationRef &animation);
    unsigned long frameIntervalFor(const AssetData::AnimationRef &animation,
                                   uint8_t versionIndex,
                                   unsigned long defaultIntervalMs);
    AssetData::BundleError firstAssetDataError() const;
    const char *firstAssetDataErrorResource() const;
    SdFat *sdCard() const;

private:
    struct AnimationState;

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
