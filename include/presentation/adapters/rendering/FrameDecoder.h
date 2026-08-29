#ifndef FRAME_DECODER_H
#define FRAME_DECODER_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include "shared/assets/BundleReader.h"

namespace FrameDecoder
{
constexpr uint16_t kDefaultAnimWidth = 128;
constexpr uint16_t kDefaultAnimHeight = 96;
constexpr uint16_t kWorkingBatchLines = 12;
constexpr size_t kDataReadBufferBytes = 1024;
constexpr size_t kLineBufferPixels = static_cast<size_t>(kDefaultAnimWidth) * kWorkingBatchLines;

void showResourceError(Adafruit_ST7735 *tft);
void showAssetDataError(Adafruit_ST7735 *tft, const char *resource);
bool showDataFrame(BundleReader &bundleReader,
                   const AssetData::AssetFrameAddress &address,
                   Adafruit_ST7735 *tft,
                   uint8_t *readBuffer,
                   size_t readBufferSize,
                   uint16_t *lineBuffer,
                   size_t lineBufferPixels,
                   int xmin,
                   int ymin,
                   int batchLines);
} // namespace FrameDecoder

#endif
