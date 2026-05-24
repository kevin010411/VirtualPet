#ifndef FRAME_DECODER_H
#define FRAME_DECODER_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <vector>
#include "render/AssetManifest.h"

namespace FrameDecoder
{
constexpr uint16_t kDefaultAnimWidth = 128;
constexpr uint16_t kDefaultAnimHeight = 96;
constexpr uint16_t kWorkingBatchLines = 4;

void showResourceError(Adafruit_ST7735 *tft);
bool endsWithIgnoreCase(const char *text, const char *suffix);
bool replaceOrAppendExtension(char *dest, size_t destSize, const char *path, const char *ext);
bool buildFramePath(char *dest, size_t destSize, const char *basePath, uint16_t frameIndex, const char *ext);
bool fileExists(SdFat *sd, const char *path);

bool showBmpImage(SdFat *sd,
                  Adafruit_ST7735 *tft,
                  std::vector<uint8_t> &rowBuffer,
                  std::vector<uint16_t> &lineBuffer,
                  const char *imgPath,
                  int xmin,
                  int ymin,
                  int batchLines);

bool showRawImage(SdFat *sd,
                  Adafruit_ST7735 *tft,
                  std::vector<uint16_t> &lineBuffer,
                  const char *imgPath,
                  uint16_t width,
                  uint16_t height,
                  int xmin,
                  int ymin,
                  int batchLines);

bool showRleImage(SdFat *sd,
                  Adafruit_ST7735 *tft,
                  std::vector<uint16_t> &lineBuffer,
                  const char *imgPath,
                  uint16_t expectedWidth,
                  uint16_t expectedHeight,
                  bool validateExpectedSize,
                  int xmin,
                  int ymin);

bool showRawFrame(SdFat *sd, Adafruit_ST7735 *tft, std::vector<uint16_t> &lineBuffer, const AnimationMeta &meta, uint16_t frameIndex);
} // namespace FrameDecoder

#endif
