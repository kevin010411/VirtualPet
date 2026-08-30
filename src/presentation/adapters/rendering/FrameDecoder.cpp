#include "presentation/adapters/rendering/FrameDecoder.h"

#include "shared/sd/SdBinaryRead.h"

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

void showAssetDataError(Adafruit_ST7735 *tft, const char *resource)
{
    if (tft == nullptr)
        return;

    tft->fillRect(0, 32, kDefaultAnimWidth, kDefaultAnimHeight, ST77XX_BLACK);
    tft->setTextColor(ST77XX_RED, ST77XX_BLACK);
    tft->setCursor(4, 64);
    tft->print(resource == nullptr || resource[0] == '\0' ? "asset data" : resource);
    tft->setCursor(8, 80);
    tft->print("resource error");
}

namespace
{
class DataBufferedReader
{
public:
    DataBufferedReader(SdBaseFile &source, uint8_t *buffer, size_t bufferSize, uint32_t byteCount)
        : file(source), readBuffer(buffer), readBufferSize(bufferSize), remainingBytes(byteCount)
    {
    }

    bool readByte(uint8_t &value)
    {
        if (remainingBytes == 0)
            return false;
        if (readPosition >= readLength && !refill())
            return false;
        value = readBuffer[readPosition++];
        --remainingBytes;
        return true;
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

    bool exhausted() const
    {
        return remainingBytes == 0;
    }

private:
    bool refill()
    {
        if (readBuffer == nullptr || readBufferSize == 0 || remainingBytes == 0)
            return false;
        size_t requested = readBufferSize;
        if (requested > remainingBytes)
            requested = static_cast<size_t>(remainingBytes);
        const int bytesRead = readSdBinary(file, readBuffer, requested);
        if (bytesRead != static_cast<int>(requested))
            return false;
        readPosition = 0;
        readLength = requested;
        return true;
    }

    SdBaseFile &file;
    uint8_t *readBuffer;
    size_t readBufferSize;
    uint32_t remainingBytes;
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

bool rejectDataFrame(BundleReader &bundleReader,
                     const AssetData::AssetFrameAddress &address,
                     AssetData::OpenFrame &frame)
{
    frame.file.close();
    bundleReader.rejectDecodedFrame(address);
    return false;
}
} // namespace


bool showDataFrame(BundleReader &bundleReader,
                   const AssetData::AssetFrameAddress &address,
                   Adafruit_ST7735 *tft,
                   uint8_t *readBuffer,
                   size_t readBufferSize,
                   uint16_t *lineBuffer,
                   size_t lineBufferPixels,
                   int xmin,
                   int ymin,
                   int batchLines)
{
    if (tft == nullptr || readBuffer == nullptr ||
        readBufferSize < kDataReadBufferBytes || lineBuffer == nullptr)
    {
        bundleReader.rejectDecodedFrame(address);
        return false;
    }

    AssetData::OpenFrame frame;
    if (!bundleReader.openFrame(address, frame))
        return false;

    const AssetData::FrameDescriptor &descriptor = frame.descriptor;
    const bool validDimensions = (descriptor.width == 128 && descriptor.height == 96) ||
                                 (descriptor.width == 32 && descriptor.height == 32);
    if (!validDimensions)
        return rejectDataFrame(bundleReader, address, frame);
    const uint32_t pixelCount = static_cast<uint32_t>(descriptor.width) * descriptor.height;
    const int safeBatchLines = batchLines < 1 ? 1 : batchLines;
    const size_t lineCapacity = static_cast<size_t>(descriptor.width) * safeBatchLines;
    if (descriptor.decodedSize != pixelCount * 2UL ||
        descriptor.encodedSize == 0 || lineCapacity > lineBufferPixels)
        return rejectDataFrame(bundleReader, address, frame);

    constexpr size_t kEncodedReadBytes = AssetData::kVerificationScratchBytes;
    uint16_t *palette = reinterpret_cast<uint16_t *>(readBuffer + kEncodedReadBytes);
    DataBufferedReader reader(frame.file, readBuffer, kEncodedReadBytes, descriptor.encodedSize);
    uint16_t paletteCount = 0;
    if (descriptor.codec == AssetData::kCodecPal8RunLiteral)
    {
        uint8_t paletteCountMinusOne = 0;
        if (!reader.readByte(paletteCountMinusOne))
            return rejectDataFrame(bundleReader, address, frame);
        paletteCount = static_cast<uint16_t>(paletteCountMinusOne) + 1;
        for (uint16_t index = 0; index < paletteCount; ++index)
        {
            if (!reader.readU16LE(palette[index]))
                return rejectDataFrame(bundleReader, address, frame);
        }
    }
    else if (descriptor.codec != AssetData::kCodecRgb565RunLiteral)
        return rejectDataFrame(bundleReader, address, frame);

    const bool frameFullyVisible = xmin >= 0 && ymin >= 0 &&
                                   static_cast<uint32_t>(xmin) + descriptor.width <= static_cast<uint32_t>(tft->width()) &&
                                   static_cast<uint32_t>(ymin) + descriptor.height <= static_cast<uint32_t>(tft->height());
    TftFrameWriteSession writeSession(tft, frameFullyVisible, xmin, ymin,
                                      descriptor.width, descriptor.height);

    uint16_t packetRemaining = 0;
    bool packetIsRun = false;
    uint16_t runPixel = 0;
    uint32_t producedPixels = 0;
    for (uint16_t batchStartRow = 0; batchStartRow < descriptor.height;
         batchStartRow = static_cast<uint16_t>(batchStartRow + safeBatchLines))
    {
        const int actualLines = batchStartRow + safeBatchLines > descriptor.height
                                    ? descriptor.height - batchStartRow
                                    : safeBatchLines;
        for (int rowOffset = 0; rowOffset < actualLines; ++rowOffset)
        {
            uint16_t *destination = &lineBuffer[static_cast<size_t>(rowOffset) * descriptor.width];
            size_t lineCount = 0;
            while (lineCount < descriptor.width)
            {
                if (packetRemaining == 0)
                {
                    uint8_t control = 0;
                    if (!reader.readByte(control))
                        return rejectDataFrame(bundleReader, address, frame);
                    packetRemaining = static_cast<uint16_t>((control & 0x7FU) + 1U);
                    packetIsRun = (control & 0x80U) != 0;
                    if (packetRemaining > pixelCount - producedPixels)
                        return rejectDataFrame(bundleReader, address, frame);
                    if (packetIsRun)
                    {
                        if (descriptor.codec == AssetData::kCodecPal8RunLiteral)
                        {
                            uint8_t paletteIndex = 0;
                            if (!reader.readByte(paletteIndex) || paletteIndex >= paletteCount)
                                return rejectDataFrame(bundleReader, address, frame);
                            runPixel = palette[paletteIndex];
                        }
                        else if (!reader.readU16LE(runPixel))
                            return rejectDataFrame(bundleReader, address, frame);
                    }
                }

                const size_t remainingLine = static_cast<size_t>(descriptor.width) - lineCount;
                const size_t copyCount = packetRemaining < remainingLine ? packetRemaining : remainingLine;
                if (packetIsRun)
                {
                    for (size_t index = 0; index < copyCount; ++index)
                        destination[lineCount + index] = runPixel;
                }
                else
                {
                    for (size_t index = 0; index < copyCount; ++index)
                    {
                        uint16_t pixel = 0;
                        if (descriptor.codec == AssetData::kCodecPal8RunLiteral)
                        {
                            uint8_t paletteIndex = 0;
                            if (!reader.readByte(paletteIndex) || paletteIndex >= paletteCount)
                                return rejectDataFrame(bundleReader, address, frame);
                            pixel = palette[paletteIndex];
                        }
                        else if (!reader.readU16LE(pixel))
                            return rejectDataFrame(bundleReader, address, frame);
                        destination[lineCount + index] = pixel;
                    }
                }

                lineCount += copyCount;
                producedPixels += static_cast<uint32_t>(copyCount);
                packetRemaining = static_cast<uint16_t>(packetRemaining - copyCount);
            }
        }

        if (writeSession.active())
            tft->writePixels(lineBuffer, static_cast<uint32_t>(descriptor.width) * actualLines);
        else
            tft->drawRGBBitmap(xmin, ymin + batchStartRow, lineBuffer, descriptor.width, actualLines);
    }

    const bool complete = producedPixels == pixelCount && packetRemaining == 0 && reader.exhausted();
    frame.file.close();
    if (!complete)
        bundleReader.rejectDecodedFrame(address);
    return complete;
}
} // namespace FrameDecoder
