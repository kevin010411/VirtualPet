#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include "resources/AssetRuntimeContract.h"
#include "common/AppProfile.h"
#if ENABLE_DEBUG
#include "display/TftDebugDisplay.h"
#endif

class AssetResources;

class Renderer
{
public:
    Renderer(Adafruit_ST7735 *ref_tft, AssetResources &resources);
    ~Renderer();

    void initAnimations();
    void setAnimationArea(uint8_t x, uint8_t y, uint8_t width = 128, uint8_t height = 96);
    bool ShowDataFrame(const AssetData::AssetFrameAddress &address,
                       int xmin = -1,
                       int ymin = -1,
                       int batch_lines = 12, uint16_t expectedWidth = 0, uint16_t expectedHeight = 0);
    bool ShowAnimationFrame(const AssetData::AnimationRef &animation,
                            uint8_t versionIndex,
                            uint16_t frameIndex,
                            int xmin = -1,
                            int ymin = -1,
                            int batchLines = 12, uint16_t expectedWidth = 0, uint16_t expectedHeight = 0);
    bool setAnimation(const AssetData::AnimationRef &animation,
                      uint8_t versionIndex,
                      bool playOnce);
    bool currentLayoutId(uint8_t &layoutId) const;
    bool willRestartAnimationLoop() const;
    bool advanceAnimationFrame();
    bool animationFrameFailed() const;
#if ENABLE_INSTRUCTION_RESPONSE
    void clearInstructionOptions();
    void showInstructionFeedback(bool correct);
#endif
    void showResourceError();
    void showResourceError(const char *resource);
#if ENABLE_DEBUG
    void showStartupResourceError(const char *fallbackResource,
                                  const char *debugStage);
#endif
#if ENABLE_DEBUG
    DebugDisplay &debugDisplay();
    void renderDebugOverlay();
    bool hasRenderedFrame() const;
#endif

private:
    struct AnimationState;
#if ENABLE_DEBUG
    void showAssetDataDebug(const char *fallbackResource = nullptr,
                            const char *debugStage = nullptr);
#endif

    Adafruit_ST7735 *tft;
    AssetResources &resources;
    AnimationState *state;
#if ENABLE_DEBUG
    TftDebugDisplay debug;
#endif
};

#endif
