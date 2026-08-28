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

    bool hasTrailingData()
    {
        uint8_t ignored = 0;
        return readByte(ignored);
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

class TftFrameWriteSession
{
public:
    TftFrameWriteSession(Adafruit_ST7735 *display,
                         bool enabled,
                         int x,
                         int y,
                         uint16_t width,
                         uint16_t height)
        : tft(enabled ? display : nullptr)
    {
        if (tft == nullptr)
            return;

        tft->startWrite();
        tft->setAddrWindow(x, y, width, height);
    }

    ~TftFrameWriteSession()
    {
        if (tft != nullptr)
            tft->endWrite();
    }

    bool active() const
    {
        return tft != nullptr;
    }

private:
    Adafruit_ST7735 *tft;
};
} // namespace

bool showRleImage(SdFat *sd,
                  Adafruit_ST7735 *tft,
                  uint8_t *readBuffer,
                  size_t readBufferSize,
                  uint16_t *lineBuffer,
                  size_t lineBufferPixels,
                  const char *imgPath,
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

    const int safeBatchLines = (batchLines < 1) ? 1 : batchLines;
    const size_t lineCapacity = static_cast<size_t>(width) * static_cast<size_t>(safeBatchLines);
    if (lineBufferPixels < lineCapacity)
    {
        frameFile.close();
        return false;
    }

    constexpr uint16_t kRunFlag = 0x8000;
    constexpr uint16_t kPacketLengthMask = 0x7FFF;
    uint16_t packetRemaining = 0;
    bool packetIsRun = false;
    uint16_t runColor = 0;
    const bool frameFullyVisible = xmin >= 0 && ymin >= 0 &&
                                   static_cast<uint32_t>(xmin) + width <= static_cast<uint32_t>(tft->width()) &&
                                   static_cast<uint32_t>(ymin) + height <= static_cast<uint32_t>(tft->height());
    TftFrameWriteSession writeSession(tft, frameFullyVisible, xmin, ymin, width, height);

    for (uint16_t batchStartRow = 0; batchStartRow < height; batchStartRow = static_cast<uint16_t>(batchStartRow + safeBatchLines))
    {
        const int actualLines = (batchStartRow + safeBatchLines > height) ? (height - batchStartRow) : safeBatchLines;

        for (int rowOffset = 0; rowOffset < actualLines; ++rowOffset)
        {
            uint16_t *destLine = &lineBuffer[static_cast<size_t>(rowOffset) * width];
            size_t lineCount = 0;

            while (lineCount < width)
            {
                if (packetRemaining == 0)
                {
                    uint16_t packetHeader = 0;
                    if (!reader.readU16LE(packetHeader))
                    {
                        frameFile.close();
                        return false;
                    }

                    packetRemaining = static_cast<uint16_t>(packetHeader & kPacketLengthMask);
                    packetIsRun = (packetHeader & kRunFlag) != 0;
                    if (packetRemaining == 0 ||
                        (packetIsRun && !reader.readU16LE(runColor)))
                    {
                        frameFile.close();
                        return false;
                    }
                }

                const size_t remainingLine = static_cast<size_t>(width) - lineCount;
                const size_t copyCount = (packetRemaining < remainingLine) ? packetRemaining : remainingLine;
                uint16_t *dest = destLine + lineCount;
                if (packetIsRun)
                {
                    for (size_t i = 0; i < copyCount; ++i)
                        *dest++ = runColor;
                }
                else
                {
                    for (size_t i = 0; i < copyCount; ++i)
                    {
                        if (!reader.readU16LE(*dest++))
                        {
                            frameFile.close();
                            return false;
                        }
                    }
                }

                lineCount += copyCount;
                packetRemaining = static_cast<uint16_t>(packetRemaining - copyCount);
            }
        }

        if (writeSession.active())
        {
            tft->writePixels(lineBuffer, static_cast<uint32_t>(width) * actualLines);
        }
        else
        {
            tft->drawRGBBitmap(xmin, ymin + batchStartRow, lineBuffer, width, actualLines);
        }
    }

    if (packetRemaining != 0 || reader.hasTrailingData())
    {
        frameFile.close();
        return false;
    }

    frameFile.close();
    return true;
}
} // namespace FrameDecoder
