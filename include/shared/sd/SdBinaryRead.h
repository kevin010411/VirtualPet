#ifndef SD_BINARY_READ_H
#define SD_BINARY_READ_H

#include <SdFat.h>
#include <stddef.h>

// Binary contracts use SdFat's base file type directly.  The Arduino File
// facade inherits Stream and retains String/numeric conversion helpers even
// though the firmware only needs exact binary reads.
inline int readSdBinary(SdBaseFile &file, void *destination, size_t size)
{
    return file.read(destination, size);
}

#endif // SD_BINARY_READ_H
