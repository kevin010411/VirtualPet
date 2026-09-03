#include "shared/integrity/Crc32.h"

uint32_t Integrity::crc32Update(uint32_t crc, const uint8_t *data, size_t length)
{
    for (size_t index = 0; index < length; ++index)
    {
        crc ^= data[index];
        for (uint8_t bit = 0; bit < 8; ++bit)
            crc = (crc & 1U) != 0 ? (crc >> 1) ^ 0xEDB88320UL : crc >> 1;
    }
    return crc;
}

uint32_t Integrity::crc32(const uint8_t *data, size_t length)
{
    return ~crc32Update(0xFFFFFFFFUL, data, length);
}
