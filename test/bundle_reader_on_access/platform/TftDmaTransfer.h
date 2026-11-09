#ifndef TEST_TFT_DMA_TRANSFER_H
#define TEST_TFT_DMA_TRANSFER_H

#if defined(STM32F103xB)

#include "SPI.h"
#include <algorithm>
#include <assert.h>

// Complete a batch only when the decoder queues the next one or ends the frame.
class TftDmaTransfer
{
public:
    explicit TftDmaTransfer(SPIClass *spi) : spi(spi) {}
    ~TftDmaTransfer() { finish(); }
    bool active() const { return spi != nullptr; }
    bool send(const uint16_t *pixels, size_t count)
    {
        assert(pixels != pending);
        finish();
        pending = pixels;
        snapshot.assign(pixels, pixels + count);
        return true;
    }
    bool finish()
    {
        if (pending == nullptr)
            return true;
        assert(std::equal(snapshot.begin(), snapshot.end(), pending));
        std::vector<uint8_t> wireBytes;
        for (const uint16_t pixel : snapshot)
        {
            wireBytes.push_back(pixel >> 8);
            wireBytes.push_back(pixel);
        }
        spi->transfer(wireBytes.data(), nullptr, wireBytes.size());
        pending = nullptr;
        return true;
    }

private:
    SPIClass *spi;
    const uint16_t *pending = nullptr;
    std::vector<uint16_t> snapshot;
};
#endif

#endif
