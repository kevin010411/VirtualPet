#ifndef FRAME_TIMING_H
#define FRAME_TIMING_H

#include "common/AppProfile.h"

#ifndef ENABLE_TFT_FRAME_TIMING
#define ENABLE_TFT_FRAME_TIMING ENABLE_DEBUG
#endif

#if ENABLE_TFT_FRAME_TIMING && defined(STM32F103xB)
#define TFT_FRAME_TIMING_ENABLED 1

#include "resources/BundleReader.h"

void recordFrameTiming(const AssetData::AssetFrameAddress &address,
                       const AssetData::FrameDescriptor &frame,
                       uint32_t tftTransferUs, uint32_t sdToFrameUs);
void flushFrameTimings(SdFat &sd, unsigned long now);
#else
#define TFT_FRAME_TIMING_ENABLED 0
#endif

#endif
