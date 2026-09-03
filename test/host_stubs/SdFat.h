#ifndef TEST_HOST_SDFAT_H
#define TEST_HOST_SDFAT_H

#include <stddef.h>
#include <stdint.h>

constexpr uint8_t FILE_READ = 0;

namespace HostSd
{
inline const uint8_t *mountedData = nullptr;
inline size_t mountedSize = 0;
}

class File
{
public:
    File() = default;
    File(const uint8_t *data, size_t size) : data_(data), size_(size) {}
    explicit operator bool() const { return data_ != nullptr; }
    bool available() const { return position_ < size_; }
    int read() { return available() ? data_[position_++] : -1; }
    int read(void *destination, size_t count) {
        if (destination == nullptr || count > size_ - position_) return -1;
        uint8_t *bytes = static_cast<uint8_t *>(destination);
        for (size_t index = 0; index < count; ++index) bytes[index] = data_[position_ + index];
        position_ += count;
        return static_cast<int>(count);
    }
    bool seek(uint32_t offset) { return seekSet(offset); }
    bool seekSet(uint32_t offset) {
        if (offset > size_) return false;
        position_ = offset;
        return true;
    }
    uint32_t size() const { return static_cast<uint32_t>(size_); }
    uint32_t fileSize() const { return static_cast<uint32_t>(size_); }
    bool open(const char *, uint8_t) {
        data_ = HostSd::mountedData;
        size_ = HostSd::mountedSize;
        position_ = 0;
        return data_ != nullptr;
    }
    bool sync() { return false; }
    void close() { data_ = nullptr; size_ = 0; position_ = 0; }
private:
    const uint8_t *data_ = nullptr;
    size_t size_ = 0;
    size_t position_ = 0;
};

class SdFat
{
public:
    SdFat() = default;
    SdFat(const uint8_t *data, size_t size) : data_(data), size_(size) {
        HostSd::mountedData = data;
        HostSd::mountedSize = size;
    }
    File open(const char *, uint8_t) { return File(data_, size_); }
private:
    const uint8_t *data_ = nullptr;
    size_t size_ = 0;
};

using SdBaseFile = File;

#endif // TEST_HOST_SDFAT_H
