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

#if ENABLE_SD_BMP_ASSETS
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
#endif

#if ENABLE_SD_RLE_ASSETS
namespace
{
constexpr size_t kRleReadBufferBytes = 512;

class RleBufferedReader
{
public:
    RleBufferedReader(File &source, std::vector<uint8_t> &buffer)
        : file(source), readBuffer(buffer)
    {
        if (readBuffer.size() < kRleReadBufferBytes)
            readBuffer.resize(kRleReadBufferBytes);
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
        const int bytesRead = file.read(readBuffer.data(), readBuffer.size());
        if (bytesRead <= 0)
            return false;

        readPosition = 0;
        readLength = static_cast<size_t>(bytesRead);
        return true;
    }

    File &file;
    std::vector<uint8_t> &readBuffer;
    size_t readPosition = 0;
    size_t readLength = 0;
};
} // namespace

bool showRleImage(SdFat *sd,
                  Adafruit_ST7735 *tft,
                  std::vector<uint8_t> &readBuffer,
                  std::vector<uint16_t> &lineBuffer,
                  const char *imgPath,
                  uint16_t expectedWidth,
                  uint16_t expectedHeight,
                  bool validateExpectedSize,
                  int xmin,
                  int ymin,
                  int batchLines)
{
    if (sd == nullptr || tft == nullptr)
        return false;

    File frameFile = sd->open(imgPath);
    if (!frameFile)
        return false;

    RleBufferedReader reader(frameFile, readBuffer);

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
    if (lineBuffer.size() < lineCapacity)
        lineBuffer.resize(lineCapacity);

    uint16_t runLength = 0;
    uint16_t runColor = 0;
    size_t pixelsWritten = 0;
    const size_t totalPixels = static_cast<size_t>(width) * height;

    for (uint16_t batchStartRow = 0; batchStartRow < height; batchStartRow = static_cast<uint16_t>(batchStartRow + safeBatchLines))
    {
        const int actualLines = (batchStartRow + safeBatchLines > height) ? (height - batchStartRow) : safeBatchLines;

        for (int rowOffset = 0; rowOffset < actualLines; ++rowOffset)
        {
            const size_t bufferRow = static_cast<size_t>(actualLines - 1 - rowOffset);
            uint16_t *destLine = &lineBuffer[bufferRow * width];
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
                for (size_t i = 0; i < copyCount; ++i)
                {
                    const size_t destX = (static_cast<size_t>(width) - 1) - (lineCount + i);
                    destLine[destX] = runColor;
                }

                lineCount += copyCount;
                pixelsWritten += copyCount;
                runLength = static_cast<uint16_t>(runLength - copyCount);
            }
        }

        const int y = static_cast<int>(height - batchStartRow - actualLines);
        tft->drawRGBBitmap(xmin, ymin + y, lineBuffer.data(), width, actualLines);
    }

    if (pixelsWritten != totalPixels || runLength != 0 || reader.hasTrailingData())
    {
        frameFile.close();
        return false;
    }

    frameFile.close();
    return true;
}
#endif
} // namespace FrameDecoder
