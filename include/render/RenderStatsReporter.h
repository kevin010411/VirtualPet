#ifndef RENDER_STATS_REPORTER_H
#define RENDER_STATS_REPORTER_H

#include <Arduino.h>
#include <SdFat.h>

struct RenderStats
{
    uint32_t totalFrames = 0;
    uint64_t totalRenderTimeUs = 0;
    uint32_t windowFrames = 0;
    uint64_t windowRenderTimeUs = 0;
    unsigned long lastReportMs = 0;
    unsigned long lastFrameMs = 0;
    uint32_t lastFrameUs = 0;
    float avgFpsTotal = 0.0f;
    float avgFpsWindow = 0.0f;
};

void updateRenderStats(RenderStats &stats, SdFat *sd, uint32_t frameRenderUs);

#endif
