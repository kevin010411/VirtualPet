#include "platform/TftDmaTransfer.h"

#if defined(STM32F103xB)

namespace
{
enum class Result : uint8_t
{
    Pending,
    Complete,
    Failed
};

constexpr uint32_t kBatchTimeoutUs = 20000;
volatile Result result = Result::Pending;
volatile uint32_t completedUs = 0;

bool waitForSpiIdle()
{
    // DMA completes before the last word leaves the SPI shifter.
    for (uint32_t attempts = 4096; attempts != 0; --attempts)
        if ((SPI2->SR & SPI_SR_BSY) == 0)
            return true;
    return false;
}
} // namespace

extern "C" void DMA1_Channel5_IRQHandler()
{
    const uint32_t flags = DMA1->ISR;
    SPI2->CR2 &= ~SPI_CR2_TXDMAEN;
    DMA1_Channel5->CCR = 0;
    DMA1->IFCR = DMA_IFCR_CGIF5;

    const bool complete = (flags & DMA_ISR_TCIF5) && !(flags & DMA_ISR_TEIF5) && waitForSpiIdle();
    completedUs = micros();
    result = complete ? Result::Complete : Result::Failed;
}

TftDmaTransfer::TftDmaTransfer(SPIClass *spi)
{
    if (spi == nullptr || spi->getHandle()->Instance != SPI2)
        return;

    // STM32 SPI2_TX uses DMA1 channel 5, the same mapping as GD32 SPI1_TX / DMA0 channel 4.
    __HAL_RCC_DMA1_CLK_ENABLE();
    DMA1_Channel5->CCR = 0;
    DMA1->IFCR = DMA_IFCR_CGIF5;
    DMA1_Channel5->CPAR = reinterpret_cast<uint32_t>(&SPI2->DR);
    NVIC_ClearPendingIRQ(DMA1_Channel5_IRQn);
    NVIC_SetPriority(DMA1_Channel5_IRQn, 3);
    NVIC_EnableIRQ(DMA1_Channel5_IRQn);

    // Keep the Arduino transaction's clock/mode and transmit native RGB565
    // halfwords MSB-first, using the MOSI pin in one-line transmit mode.
    savedControl = SPI2->CR1;
    SPI2->CR1 = savedControl & ~SPI_CR1_SPE;
    SPI2->CR1 = (savedControl & ~(SPI_CR1_SPE | SPI_CR1_RXONLY)) |
                SPI_CR1_DFF | SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE;
    SPI2->CR1 |= SPI_CR1_SPE;
    configured = true;
}

TftDmaTransfer::~TftDmaTransfer()
{
    finish();
}

bool TftDmaTransfer::waitForBatch()
{
    if (!pending)
        return true;
    while (result == Result::Pending && micros() - startedUs < kBatchTimeoutUs)
    {
    }
    pending = false;
    if (result != Result::Complete)
        return false;
    elapsedUs += completedUs - startedUs;
    return true;
}

bool TftDmaTransfer::send(const uint16_t *pixels, size_t count)
{
    if (!waitForBatch())
        return false;
    DMA1_Channel5->CMAR = reinterpret_cast<uint32_t>(pixels);
    DMA1_Channel5->CNDTR = count;
    result = Result::Pending;
    pending = true;
    __DMB();
    startedUs = micros();
    DMA1_Channel5->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 |
                         DMA_CCR_MSIZE_0 | DMA_CCR_TCIE | DMA_CCR_TEIE | DMA_CCR_EN;
    SPI2->CR2 |= SPI_CR2_TXDMAEN;
    return true;
}

bool TftDmaTransfer::finish()
{
    if (!configured)
        return true;
    const bool complete = waitForBatch();
    NVIC_DisableIRQ(DMA1_Channel5_IRQn);
    SPI2->CR2 &= ~SPI_CR2_TXDMAEN;
    DMA1_Channel5->CCR = 0;
    DMA1->IFCR = DMA_IFCR_CGIF5;
    NVIC_ClearPendingIRQ(DMA1_Channel5_IRQn);
    const bool idle = waitForSpiIdle();
    SPI2->CR1 &= ~SPI_CR1_SPE;
    SPI2->CR1 = savedControl & ~SPI_CR1_SPE;
    SPI2->CR1 = savedControl;
    configured = false;
    return complete && idle;
}

#endif
