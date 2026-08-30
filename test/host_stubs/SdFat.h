#ifndef TEST_HOST_SDFAT_H
#define TEST_HOST_SDFAT_H

#include <stddef.h>
#include <stdint.h>

constexpr uint8_t FILE_READ = 0;

class File
{
public:
    explicit operator bool() const { return false; }
    bool available() const { return false; }
    int read() { return -1; }
    int read(void *, size_t) { return -1; }
    bool seek(uint32_t) { return false; }
    uint32_t size() const { return 0; }
    void close() {}
};

class SdFat
{
public:
    File open(const char *, uint8_t) { return File(); }
};

#endif // TEST_HOST_SDFAT_H
