#include "presentation/adapters/rendering/FrameDecoder.h"

#include <string.h>
#include "shared/utils/TextBuffer.h"

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

void showPathError(Adafruit_ST7735 *tft)
{
    if (tft == nullptr)
        return;

    tft->fillRect(0, 32, kDefaultAnimWidth, kDefaultAnimHeight, ST77XX_BLACK);
    tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft->setCursor(8, 72);
    tft->print("path error");
}

void showRegistryFullError(Adafruit_ST7735 *tft)
{
    if (tft == nullptr)
        return;

    tft->fillRect(0, 32, kDefaultAnimWidth, kDefaultAnimHeight, ST77XX_BLACK);
    tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft->setCursor(8, 72);
    tft->print("registry full");
}

void showStatusNotFound(Adafruit_ST7735 *tft)
{
    if (tft == nullptr)
        return;

    tft->fillRect(0, 32, kDefaultAnimWidth, kDefaultAnimHeight, ST77XX_BLACK);
    tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft->setCursor(8, 72);
    tft->print("status not found");
}

void showInitPetNotExist(Adafruit_ST7735 *tft)
{
    if (tft == nullptr)
        return;

    tft->fillScreen(ST77XX_BLACK);
    tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft->setCursor(8, 72);
    tft->print("init pet not exist");
}

void showPetBehaviorLoadingError(Adafruit_ST7735 *tft)
{
    if (tft == nullptr)
        return;

    tft->fillScreen(ST77XX_BLACK);
    tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft->setCursor(0, 32);
    tft->print("sd card config loading error");
}

bool replaceOrAppendExtension(char *dest, size_t destSize, const char *path, const char *ext)
{
    if (dest == nullptr || destSize == 0 || path == nullptr || ext == nullptr)
        return false;

    const char *dot = strrchr(path, '.');
    const char *slash = strrchr(path, '/');
    const bool hasExt = (dot != nullptr) && (slash == nullptr || dot > slash);

    TextBuffer result(dest, destSize);
    return hasExt ? result.append(path, static_cast<size_t>(dot - path)) && result.append(ext) && result.ok()
                  : result.append(path) && result.append(ext) && result.ok();
}

bool buildFramePath(char *dest, size_t destSize, const char *basePath, uint16_t frameIndex, const char *ext)
{
    if (dest == nullptr || destSize == 0 || basePath == nullptr || ext == nullptr)
        return false;

    TextBuffer result(dest, destSize);
    return result.append(basePath) && result.append("/") && result.appendUnsigned(frameIndex) && result.append(ext) && result.ok();
}

namespace
{
class RleBufferedReader
{
public:
    RleBufferedReader(File &source, uint8_t *buffer, size_t bufferSize)
        : file(source), readBuffer(buffer)
    {
        readBufferSize = bufferSize;
    }

    bool readU16LE(uint16_t &value)
    {
        uint8_t low = 0;
        uint8_t high = 0;
        if (!readByte(low) || !readByte(high))
            return false;

        value = static_cast<uint16_t>(low | (static_cast<uint16_t>(high) << 8));
        return true;
    }

    bool hasTrailingData() const
    {
        return readPosition < readLength || file.available();
    }

private:
    bool readByte(uint8_t &value)
    {
        if (readPosition >= readLength && !refill())
            return false;

        value = readBuffer[readPosition++];
        return true;
    }

    bool refill()
    {
        if (readBuffer == nullptr || readBufferSize == 0)
            return false;

        const int bytesRead = file.read(readBuffer, readBufferSize);
        if (bytesRead <= 0)
            return false;

        readPosition = 0;
        readLength = static_cast<size_t>(bytesRead);
        return true;
    }

    File &file;
    uint8_t *readBuffer;
    size_t readBufferSize = 0;
    size_t readPosition = 0;
    size_t readLength = 0;
};
} // namespace

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
                  int batchLines)
{
    if (sd == nullptr || tft == nullptr || readBuffer == nullptr || lineBuffer == nullptr)
        return false;

    File frameFile = sd->open(imgPath);
    if (!frameFile)
        return false;

    RleBufferedReader reader(frameFile, readBuffer, readBufferSize);

    uint16_t width = 0;
    uint16_t height = 0;
    if (!reader.readU16LE(width) || !reader.readU16LE(height))
    {
        frameFile.close();
        return false;
    }

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

    const int safeBatchLines = (batchLines < 1) ? 1 : batchLines;
    const size_t lineCapacity = static_cast<size_t>(width) * static_cast<size_t>(safeBatchLines);
    if (lineBufferPixels < lineCapacity)
    {
        frameFile.close();
        return false;
    }

    uint16_t runLength = 0;
    uint16_t runColor = 0;
    size_t pixelsWritten = 0;
    const size_t totalPixels = static_cast<size_t>(width) * height;

    for (uint16_t batchStartRow = 0; batchStartRow < height; batchStartRow = static_cast<uint16_t>(batchStartRow + safeBatchLines))
    {
        const int actualLines = (batchStartRow + safeBatchLines > height) ? (height - batchStartRow) : safeBatchLines;

        for (int rowOffset = 0; rowOffset < actualLines; ++rowOffset)
        {
            uint16_t *destLine = &lineBuffer[static_cast<size_t>(rowOffset) * width];
            size_t lineCount = 0;

            while (lineCount < width)
            {
                if (runLength == 0)
                {
                    if (!reader.readU16LE(runLength) || !reader.readU16LE(runColor))
                    {
                        frameFile.close();
                        return false;
                    }

                    if (runLength == 0)
                    {
                        frameFile.close();
                        return false;
                    }
                }

                const size_t remainingLine = static_cast<size_t>(width) - lineCount;
                const size_t copyCount = (runLength < remainingLine) ? runLength : remainingLine;
                uint16_t *dest = destLine + lineCount;
                for (size_t i = 0; i < copyCount; ++i)
                    *dest++ = runColor;

                lineCount += copyCount;
                pixelsWritten += copyCount;
                runLength = static_cast<uint16_t>(runLength - copyCount);
            }
        }

        tft->drawRGBBitmap(xmin, ymin + batchStartRow, lineBuffer, width, actualLines);
    }

    if (pixelsWritten != totalPixels || runLength != 0 || reader.hasTrailingData())
    {
        frameFile.close();
        return false;
    }

    frameFile.close();
    return true;
}
} // namespace FrameDecoder
