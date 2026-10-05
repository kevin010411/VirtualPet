#ifndef SHARED_INTEGRITY_CRC32_H
#define SHARED_INTEGRITY_CRC32_H

#include <stddef.h>
#include <stdint.h>

namespace Integrity
{
uint32_t crc32Update(uint32_t crc, const uint8_t *data, size_t length);
uint32_t crc32(const uint8_t *data, size_t length);
}

#endif
