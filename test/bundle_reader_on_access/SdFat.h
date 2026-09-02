#ifndef BUNDLE_READER_ON_ACCESS_SDFAT_H
#define BUNDLE_READER_ON_ACCESS_SDFAT_H

#include <stddef.h>
#include <stdint.h>

constexpr uint8_t FILE_READ = 0;

namespace BundleReaderHostSd
{
inline const uint8_t *mountedData = nullptr;
inline size_t mountedSize = 0;
}

class File
{
public:
    explicit operator bool() const { return data_ != nullptr; }
    int read(void *destination, size_t count)
    {
        if (destination == nullptr || count > size_ - position_)
            return -1;
        uint8_t *bytes = static_cast<uint8_t *>(destination);
        for (size_t index = 0; index < count; ++index)
            bytes[index] = data_[position_ + index];
        position_ += count;
        return static_cast<int>(count);
    }
    bool seekSet(uint32_t offset)
    {
        if (offset > size_)
            return false;
        position_ = offset;
        return true;
    }
    uint32_t fileSize() const { return static_cast<uint32_t>(size_); }
    bool open(const char *, uint8_t)
    {
        data_ = BundleReaderHostSd::mountedData;
        size_ = BundleReaderHostSd::mountedSize;
        position_ = 0;
        return data_ != nullptr;
    }
    void close()
    {
        data_ = nullptr;
        size_ = 0;
        position_ = 0;
    }

private:
    const uint8_t *data_ = nullptr;
    size_t size_ = 0;
    size_t position_ = 0;
};

class SdFat
{
public:
    SdFat(const uint8_t *data, size_t size)
    {
        BundleReaderHostSd::mountedData = data;
        BundleReaderHostSd::mountedSize = size;
    }
};

using SdBaseFile = File;

#endif // BUNDLE_READER_ON_ACCESS_SDFAT_H
