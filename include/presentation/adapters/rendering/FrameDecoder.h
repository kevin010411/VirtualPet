#ifndef FRAME_DECODER_H
#define FRAME_DECODER_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include "shared/assets/AssetManifest.h"

namespace FrameDecoder
{
constexpr uint16_t kDefaultAnimWidth = 128;
constexpr uint16_t kDefaultAnimHeight = 96;
constexpr uint16_t kWorkingBatchLines = 12;
constexpr size_t kRleReadBufferBytes = 1024;
constexpr size_t kLineBufferPixels = static_cast<size_t>(kDefaultAnimWidth) * kWorkingBatchLines;

void showResourceError(Adafruit_ST7735 *tft);
void showPathError(Adafruit_ST7735 *tft);
void showRegistryFullError(Adafruit_ST7735 *tft);
void showStatusNotFound(Adafruit_ST7735 *tft);
void showInitPetNotExist(Adafruit_ST7735 *tft);
void showConfigLoadingError(Adafruit_ST7735 *tft, const char *resource);
bool replaceOrAppendExtension(char *dest, size_t destSize, const char *path, const char *ext);
bool buildFramePath(char *dest, size_t destSize, const char *basePath, uint16_t frameIndex, const char *ext);

bool showRleImage(SdFat *sd,
                  Adafruit_ST7735 *tft,
                  uint8_t *readBuffer,
                  size_t readBufferSize,
                  uint16_t *lineBuffer,
                  size_t lineBufferPixels,
                  const char *imgPath,
                  uint16_t expectedWidth,
                  uint16_t expectedHeight,
                  bool validateExpectedSize,
                  int xmin,
                  int ymin,
                  int batchLines);
} // namespace FrameDecoder

#endif
