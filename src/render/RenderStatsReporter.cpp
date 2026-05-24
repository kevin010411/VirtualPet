#include "render/RenderStatsReporter.h"

#include <stdio.h>

namespace
{
bool writeFpsReport(SdFat *sd, const RenderStats &stats)
{
    if (sd == nullptr)
        return false;

    File f = sd->open("/render_fps.tmp", FILE_WRITE);
    if (!f)
        return false;

    char line[96];
    snprintf(line, sizeof(line), "total_frames=%lu\n", static_cast<unsigned long>(stats.totalFrames));
    f.print(line);
    snprintf(line, sizeof(line), "avg_fps_total=%.2f\n", stats.avgFpsTotal);
    f.print(line);
    snprintf(line, sizeof(line), "avg_fps_window=%.2f\n", stats.avgFpsWindow);
    f.print(line);
    snprintf(line, sizeof(line), "last_frame_ms=%lu\n", stats.lastFrameMs);
    f.print(line);
    snprintf(line, sizeof(line), "last_frame_us=%lu\n", static_cast<unsigned long>(stats.lastFrameUs));
    f.print(line);
    f.flush();
    f.close();

    if (sd->exists("/render_fps.txt"))
        sd->remove("/render_fps.txt");
    return sd->rename("/render_fps.tmp", "/render_fps.txt");
}
} // namespace

void updateRenderStats(RenderStats &stats, SdFat *sd, uint32_t frameRenderUs)
{
    stats.totalFrames += 1;
    stats.windowFrames += 1;
    stats.totalRenderTimeUs += frameRenderUs;
    stats.windowRenderTimeUs += frameRenderUs;
    stats.lastFrameUs = frameRenderUs;
    stats.lastFrameMs = millis();

    if (stats.totalRenderTimeUs > 0)
        stats.avgFpsTotal = (1000000.0f * stats.totalFrames) / static_cast<float>(stats.totalRenderTimeUs);
    if (stats.windowRenderTimeUs > 0)
        stats.avgFpsWindow = (1000000.0f * stats.windowFrames) / static_cast<float>(stats.windowRenderTimeUs);

    const bool reportDueByTime = (stats.lastFrameMs - stats.lastReportMs) >= 5000UL;
    const bool reportDueByFrames = stats.windowFrames >= 100;
    if (!reportDueByTime && !reportDueByFrames)
        return;

    writeFpsReport(sd, stats);
    stats.lastReportMs = stats.lastFrameMs;
    stats.windowFrames = 0;
    stats.windowRenderTimeUs = 0;
}
