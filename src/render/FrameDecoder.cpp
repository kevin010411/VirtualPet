#include "render/FrameDecoder.h"

#include <stdio.h>
#include <string.h>

namespace FrameDecoder
{
void showResourceError(Adafruit_ST7735 *tft)
{
    if (tft == nullptr)
        return;

    tft->fillRect(0, 32, kDefaultAnimWidth, kDefaultAnimHeight, ST77XX_BLACK);
    tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft->setCursor(8, 72);
    tft->print("resource error");
}

bool endsWithIgnoreCase(const char *text, const char *suffix)
{
    if (text == nullptr || suffix == nullptr)
        return false;

    const size_t textLen = strlen(text);
    const size_t suffixLen = strlen(suffix);
    if (textLen < suffixLen)
        return false;

    const char *start = text + textLen - suffixLen;
    for (size_t i = 0; i < suffixLen; ++i)
    {
        char a = start[i];
        char b = suffix[i];
        if (a >= 'A' && a <= 'Z')
            a = static_cast<char>(a - 'A' + 'a');
        if (b >= 'A' && b <= 'Z')
            b = static_cast<char>(b - 'A' + 'a');
        if (a != b)
            return false;
    }
    return true;
}

bool replaceOrAppendExtension(char *dest, size_t destSize, const char *path, const char *ext)
{
    if (dest == nullptr || destSize == 0 || path == nullptr || ext == nullptr)
        return false;

    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    const bool hasExt = (dot != nullptr) && (slash == nullptr || dot > slash);

    if (hasExt)
        return snprintf(dest, destSize, "%.*s%s", static_cast<int>(dot - path), path, ext) < static_cast<int>(destSize);

    return snprintf(dest, destSize, "%s%s", path, ext) < static_cast<int>(destSize);
}

bool buildFramePath(char *dest, size_t destSize, const char *basePath, uint16_t frameIndex, const char *ext)
{
    return snprintf(dest, destSize, "%s/%u%s", basePath, static_cast<unsigned>(frameIndex), ext) < static_cast<int>(destSize);
}

bool fileExists(SdFat *sd, const char *path)
{
    if (sd == nullptr || path == nullptr)
        return false;
    return sd->exists(path);
}

bool showBmpImage(SdFat *sd,
                  Adafruit_ST7735 *tft,
                  std::vector<uint8_t> &rowBuffer,
                  std::vector<uint16_t> &lineBuffer,
                  const char *imgPath,
                  int xmin,
                  int ymin,
                  int batchLines)
{
    File bmpFile = sd->open(imgPath);
    if (!bmpFile)
        return false;

    if (bmpFile.read() != 'B' || bmpFile.read() != 'M')
    {
        bmpFile.close();
        return false;
    }

    bmpFile.seek(10);
    const uint32_t pixelDataOffset = bmpFile.read() |
                                     (bmpFile.read() << 8) |
                                     (bmpFile.read() << 16) |
                                     (bmpFile.read() << 24);

    bmpFile.seek(18);
    const int32_t bmpWidth = bmpFile.read() |
                             (bmpFile.read() << 8) |
                             (bmpFile.read() << 16) |
                             (bmpFile.read() << 24);
    const int32_t bmpHeight = bmpFile.read() |
                              (bmpFile.read() << 8) |
                              (bmpFile.read() << 16) |
                              (bmpFile.read() << 24);

    bmpFile.seek(28);
    const uint16_t bitsPerPixel = bmpFile.read() | (bmpFile.read() << 8);
    if (bitsPerPixel != 24 || bmpWidth <= 0 || bmpHeight <= 0)
    {
        bmpFile.close();
        return false;
    }

    const int rowSize = ((bmpWidth * 3 + 3) / 4) * 4;
    const size_t rowCapacity = static_cast<size_t>(rowSize) * batchLines;
    const size_t lineCapacity = static_cast<size_t>(bmpWidth) * batchLines;
    if (rowBuffer.size() < rowCapacity)
        rowBuffer.resize(rowCapacity);
    if (lineBuffer.size() < lineCapacity)
        lineBuffer.resize(lineCapacity);

    bmpFile.seek(pixelDataOffset);
    for (int y = 0; y < bmpHeight; y += batchLines)
    {
        const int actualLines = (y + batchLines > bmpHeight) ? (bmpHeight - y) : batchLines;
        for (int i = 0; i < actualLines; i++)
        {
            const int bytesRead = bmpFile.read(&rowBuffer[i * rowSize], rowSize);
            if (bytesRead != rowSize)
            {
                bmpFile.close();
                return false;
            }

            for (int x = 0; x < bmpWidth; x++)
            {
                const int j = x * 3;
                const uint8_t b = rowBuffer[i * rowSize + j];
                const uint8_t g = rowBuffer[i * rowSize + j + 1];
                const uint8_t r = rowBuffer[i * rowSize + j + 2];
                const int destX = bmpWidth - 1 - x;
                lineBuffer[i * bmpWidth + destX] = ((r & 0xF8) << 8) |
                                                   ((g & 0xFC) << 3) |
                                                   (b >> 3);
            }
        }

        for (int i = 0; i < actualLines; i++)
            tft->drawRGBBitmap(xmin, ymin + y + i, &lineBuffer[i * bmpWidth], bmpWidth, 1);
    }

    bmpFile.close();
    return true;
}

bool showRawImage(SdFat *sd,
                  Adafruit_ST7735 *tft,
                  std::vector<uint16_t> &lineBuffer,
                  const char *imgPath,
                  uint16_t width,
                  uint16_t height,
                  int xmin,
                  int ymin,
                  int batchLines)
{
    File frameFile = sd->open(imgPath);
    if (!frameFile)
        return false;

    const size_t lineCapacity = static_cast<size_t>(width) * batchLines;
    if (lineBuffer.size() < lineCapacity)
        lineBuffer.resize(lineCapacity);

    for (uint16_t y = 0; y < height; y += batchLines)
    {
        const uint16_t actualLines = (y + batchLines > height) ? (height - y) : static_cast<uint16_t>(batchLines);
        const size_t bytesNeeded = static_cast<size_t>(width) * actualLines * sizeof(uint16_t);
        const int bytesRead = frameFile.read(reinterpret_cast<uint8_t *>(lineBuffer.data()), bytesNeeded);
        if (bytesRead != static_cast<int>(bytesNeeded))
        {
            frameFile.close();
            return false;
        }

        for (uint16_t i = 0; i < actualLines; ++i)
            tft->drawRGBBitmap(xmin, ymin + y + i, &lineBuffer[i * width], width, 1);
    }

    frameFile.close();
    return true;
}

bool showRleImage(SdFat *sd,
                  Adafruit_ST7735 *tft,
                  std::vector<uint16_t> &lineBuffer,
                  const char *imgPath,
                  uint16_t expectedWidth,
                  uint16_t expectedHeight,
                  bool validateExpectedSize,
                  int xmin,
                  int ymin)
{
    File frameFile = sd->open(imgPath);
    if (!frameFile)
        return false;

    const int widthLo = frameFile.read();
    const int widthHi = frameFile.read();
    const int heightLo = frameFile.read();
    const int heightHi = frameFile.read();
    if (widthLo < 0 || widthHi < 0 || heightLo < 0 || heightHi < 0)
    {
        frameFile.close();
        return false;
    }

    const uint16_t width = static_cast<uint16_t>(widthLo | (widthHi << 8));
    const uint16_t height = static_cast<uint16_t>(heightLo | (heightHi << 8));
    if (width == 0 || height == 0)
    {
        frameFile.close();
        return false;
    }

    if (validateExpectedSize && (width != expectedWidth || height != expectedHeight))
    {
        frameFile.close();
        return false;
    }

    const size_t lineCapacity = static_cast<size_t>(width);
    if (lineBuffer.size() < lineCapacity)
        lineBuffer.resize(lineCapacity);

    uint16_t runLength = 0;
    uint16_t runColor = 0;
    size_t pixelsWritten = 0;
    const size_t totalPixels = static_cast<size_t>(width) * height;

    while (pixelsWritten < totalPixels)
    {
        const int rowIndex = static_cast<int>(pixelsWritten / width);
        size_t lineCount = 0;
        while (lineCount < width)
        {
            if (runLength == 0)
            {
                const int lowCount = frameFile.read();
                const int highCount = frameFile.read();
                const int lowColor = frameFile.read();
                const int highColor = frameFile.read();
                if (lowCount < 0 || highCount < 0 || lowColor < 0 || highColor < 0)
                {
                    frameFile.close();
                    return false;
                }

                runLength = static_cast<uint16_t>(lowCount | (highCount << 8));
                runColor = static_cast<uint16_t>(lowColor | (highColor << 8));
                if (runLength == 0)
                {
                    frameFile.close();
                    return false;
                }
            }

            const size_t copyCount = (runLength < (width - lineCount)) ? runLength : (width - lineCount);
            for (size_t i = 0; i < copyCount; ++i)
            {
                const size_t destX = (width - 1) - (lineCount + i);
                lineBuffer[destX] = runColor;
            }

            lineCount += copyCount;
            pixelsWritten += copyCount;
            runLength = static_cast<uint16_t>(runLength - copyCount);
        }

        const int y = static_cast<int>((height - 1) - rowIndex);
        tft->drawRGBBitmap(xmin, ymin + y, lineBuffer.data(), width, 1);
    }

    if (runLength != 0 || frameFile.available())
    {
        frameFile.close();
        return false;
    }

    frameFile.close();
    return true;
}

bool showRawFrame(SdFat *sd, Adafruit_ST7735 *tft, std::vector<uint16_t> &lineBuffer, const AnimationMeta &meta, uint16_t frameIndex)
{
    char path[128];
    if (!buildFramePath(path, sizeof(path), meta.path, frameIndex, ".raw"))
        return false;
    return showRawImage(sd, tft, lineBuffer, path, meta.width, meta.height, 0, 32, kWorkingBatchLines);
}
} // namespace FrameDecoder
