#ifndef RENDERER_H
#define RENDERER_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include "Animation.h"

class Renderer
{
public:
    Renderer(Adafruit_ST7735 *ref_tft, SdFat *ref_SD);
    ~Renderer();

    void initAnimations();
    bool ShowSDCardImage(const char *img_path, int xmin = 0, int ymin = 0, int batch_lines = 4);
    bool setAnimation(AnimationId id, bool playOnce);
    bool advanceAnimationFrame();
    unsigned long frameIntervalFor(AnimationId id, unsigned long defaultIntervalMs) const;

private:
    struct AnimationState;

    Adafruit_ST7735 *tft;
    SdFat *SD;
    AnimationState *state;
};

#endif
