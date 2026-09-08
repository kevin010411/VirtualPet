#ifndef SD_CARD_RUNTIME_SDFAT_H
#define SD_CARD_RUNTIME_SDFAT_H

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <memory>
#include <iterator>
#include <string>
#include <vector>

constexpr uint8_t FILE_READ = 0;

namespace SdCardRuntimeHost
{
inline std::filesystem::path root;
}

class File
{
public:
    explicit operator bool() const { return bytes_ != nullptr; }

    int read(void *destination, size_t count)
    {
        if (destination == nullptr || bytes_ == nullptr || count > bytes_->size() - position_)
            return -1;
        std::memcpy(destination, bytes_->data() + position_, count);
        position_ += count;
        return static_cast<int>(count);
    }

    bool seekSet(uint32_t offset)
    {
        if (bytes_ == nullptr || offset > bytes_->size())
            return false;
        position_ = offset;
        return true;
    }

    uint32_t fileSize() const
    {
        return bytes_ == nullptr ? 0 : static_cast<uint32_t>(bytes_->size());
    }

    bool open(const char *path, uint8_t)
    {
        close();
        if (path == nullptr)
            return false;
        std::string relative(path);
        while (!relative.empty() && (relative.front() == '/' || relative.front() == '\\'))
            relative.erase(relative.begin());
        std::ifstream input(SdCardRuntimeHost::root / relative, std::ios::binary);
        if (!input)
            return false;
        bytes_ = std::make_shared<std::vector<uint8_t>>(
            std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        return true;
    }

    void close()
    {
        bytes_.reset();
        position_ = 0;
    }

private:
    std::shared_ptr<std::vector<uint8_t>> bytes_;
    size_t position_ = 0;
};

class SdFat
{
public:
    explicit SdFat(const char *root) { SdCardRuntimeHost::root = root; }
};

using SdBaseFile = File;

#endif
