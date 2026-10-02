#ifndef CUSTOM_LAYOUT_HOST_SD_H
#define CUSTOM_LAYOUT_HOST_SD_H
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
#include <cstring>
#include <stdint.h>
constexpr uint8_t FILE_READ = 0;
namespace CustomLayoutHost { inline std::string root; }
class File
{
public:
    bool open(const char *path, uint8_t)
    {
        std::ifstream input(CustomLayoutHost::root + path, std::ios::binary);
        if (!input) return false;
        data.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
        position = 0;
        return true;
    }
    int read(void *destination, size_t count)
    {
        if (count > data.size() - position) return -1;
        memcpy(destination, data.data() + position, count);
        position += count;
        return static_cast<int>(count);
    }
    bool seekSet(uint32_t offset)
    {
        if (offset > data.size()) return false;
        position = offset;
        return true;
    }
    uint32_t fileSize() const { return static_cast<uint32_t>(data.size()); }
    void close() { data.clear(); position = 0; }
private:
    std::vector<uint8_t> data;
    size_t position = 0;
};
using SdBaseFile = File;
class SdFat {};
#endif
