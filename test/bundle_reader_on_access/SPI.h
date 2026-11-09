#ifndef BUNDLE_READER_ON_ACCESS_SPI_H
#define BUNDLE_READER_ON_ACCESS_SPI_H

#include <stddef.h>
#include <stdint.h>
#include <vector>

class SPIClass
{
public:
    void transfer(const void *buffer, void *, size_t count)
    {
        const auto *data = static_cast<const uint8_t *>(buffer);
        bytes.insert(bytes.end(), data, data + count);
        transferSizes.push_back(count);
    }

    std::vector<uint8_t> bytes;
    std::vector<size_t> transferSizes;
};

#endif
