#include "display/FrameTiming.h"

#if TFT_FRAME_TIMING_ENABLED

#include <stdio.h>

namespace
{
constexpr uint8_t kSamplesPerSize = 8;
constexpr unsigned long kFlushIntervalMs = 2000;
#if defined(PLATFORM_MCU_GD32F103CBT6) && PLATFORM_MCU_GD32F103CBT6
constexpr char kTimingPath[] = "/tft_timing_dma.csv";
#else
constexpr char kTimingPath[] = "/tft_timing_stm32_dma.csv";
#endif

struct Sample
{
    AssetData::AssetFrameAddress address;
    uint16_t width;
    uint16_t height;
    uint32_t encodedBytes;
    uint32_t tftTransferUs;
    uint32_t sdToFrameUs;
};

Sample samples[2 * kSamplesPerSize];
uint8_t sampleCount = 0;
uint8_t fullScreenCount = 0;
uint8_t savedCount = 0;
unsigned long lastFlushAttempt = 0;
} // namespace

void recordFrameTiming(const AssetData::AssetFrameAddress &address,
                       const AssetData::FrameDescriptor &frame,
                       uint32_t tftTransferUs, uint32_t sdToFrameUs)
{
    const bool fullScreen = frame.width == 128 && frame.height == 160;
    const uint8_t sizeCount = fullScreen ? fullScreenCount : sampleCount - fullScreenCount;
    if (sizeCount >= kSamplesPerSize)
        return;
    samples[sampleCount++] = {address, frame.width, frame.height, frame.encodedSize,
                              tftTransferUs, sdToFrameUs};
    if (fullScreen)
        ++fullScreenCount;
}

void flushFrameTimings(SdFat &sd, unsigned long now)
{
    if (savedCount == sampleCount || now - lastFlushAttempt < kFlushIntervalMs)
        return;
    lastFlushAttempt = now;

    File file = sd.open(kTimingPath, O_WRONLY | O_CREAT | O_TRUNC);
    if (!file)
        return;
    constexpr char header[] =
        "species,outfit,animation,version,frame,width,height,encoded_bytes,tft_transfer_us,sd_to_frame_us\n";
    bool ok = file.write(reinterpret_cast<const uint8_t *>(header), sizeof(header) - 1) == sizeof(header) - 1;
    for (uint8_t index = 0; ok && index < sampleCount; ++index)
    {
        const Sample &sample = samples[index];
        char row[96];
        const int length = snprintf(row, sizeof(row), "%u,%u,%u,%u,%u,%u,%u,%lu,%lu,%lu\n",
                                    unsigned(sample.address.speciesSlot), unsigned(sample.address.outfitSlot),
                                    unsigned(sample.address.animationId), unsigned(sample.address.versionIndex),
                                    unsigned(sample.address.frameIndex), unsigned(sample.width), unsigned(sample.height),
                                    static_cast<unsigned long>(sample.encodedBytes),
                                    static_cast<unsigned long>(sample.tftTransferUs),
                                    static_cast<unsigned long>(sample.sdToFrameUs));
        ok = length > 0 && static_cast<size_t>(length) < sizeof(row) &&
             file.write(reinterpret_cast<const uint8_t *>(row), length) == static_cast<size_t>(length);
    }
    if (file.close() && ok)
        savedCount = sampleCount;
}

#endif
