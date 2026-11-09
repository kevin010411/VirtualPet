#ifndef TFT_DMA_TRANSFER_H
#define TFT_DMA_TRANSFER_H

#if defined(STM32F103xB)
#include <SPI.h>

class TftDmaTransfer
{
public:
    explicit TftDmaTransfer(SPIClass *spi);
    ~TftDmaTransfer();
    TftDmaTransfer(const TftDmaTransfer &) = delete;
    TftDmaTransfer &operator=(const TftDmaTransfer &) = delete;

    bool active() const { return configured; }
    bool send(const uint16_t *pixels, size_t count);
    bool finish();
    uint32_t transferUs() const { return elapsedUs; }

private:
    bool waitForBatch();

    uint32_t savedControl = 0;
    uint32_t startedUs = 0;
    uint32_t elapsedUs = 0;
    bool configured = false;
    bool pending = false;
};
#endif

#endif
