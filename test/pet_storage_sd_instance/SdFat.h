#ifndef PET_STORAGE_SD_INSTANCE_SDFAT_H
#define PET_STORAGE_SD_INSTANCE_SDFAT_H

#include <stddef.h>
#include <stdint.h>
#include <map>
#include <memory>
#include <string>
#include <vector>

constexpr uint8_t FILE_READ = 0x01;
constexpr uint8_t O_WRONLY = 0x02;
constexpr uint8_t O_CREAT = 0x04;
constexpr uint8_t O_TRUNC = 0x08;
constexpr uint8_t O_RDONLY = FILE_READ;
constexpr uint8_t FS_ATTRIB_READ_ONLY = 0x01;

struct HostFileEntry
{
    std::vector<uint8_t> bytes;
    bool readOnly = false;
};

class File
{
public:
    File() = default;
    explicit File(std::shared_ptr<HostFileEntry> entry,
                  size_t writeLimit = static_cast<size_t>(-1),
                  bool syncOk = true)
        : entry_(entry), writeLimit_(writeLimit), syncOk_(syncOk) {}

    explicit operator bool() const { return entry_ != nullptr; }
    size_t fileSize() const { return entry_ == nullptr ? 0 : entry_->bytes.size(); }
    int read(void *destination, size_t count)
    {
        if (entry_ == nullptr || position_ + count > entry_->bytes.size())
            return -1;
        uint8_t *output = static_cast<uint8_t *>(destination);
        for (size_t index = 0; index < count; ++index)
            output[index] = entry_->bytes[position_ + index];
        position_ += count;
        return static_cast<int>(count);
    }
    size_t write(const uint8_t *source, size_t count)
    {
        if (entry_ == nullptr || entry_->readOnly)
            return 0;
        const size_t actual = count < writeLimit_ ? count : writeLimit_;
        entry_->bytes.assign(source, source + actual);
        position_ = actual;
        return actual;
    }
    bool sync() { return entry_ != nullptr && syncOk_; }
    int attrib() const { return entry_ == nullptr ? -1 : (entry_->readOnly ? FS_ATTRIB_READ_ONLY : 0); }
    bool attrib(uint8_t attributes)
    {
        if (entry_ == nullptr || !syncOk_)
            return false;
        entry_->readOnly = (attributes & FS_ATTRIB_READ_ONLY) != 0;
        return true;
    }
    void close() {}

private:
    std::shared_ptr<HostFileEntry> entry_;
    size_t position_ = 0;
    size_t writeLimit_ = static_cast<size_t>(-1);
    bool syncOk_ = true;
};

class SdFat;

class SdBaseFile
{
public:
    bool open(const char *, uint8_t) { return false; }
    bool open(SdFat *sd, const char *path, uint8_t flags);
    explicit operator bool() const { return static_cast<bool>(file_); }
    size_t fileSize() const { return file_.fileSize(); }
    int read(void *destination, size_t count) { return file_.read(destination, count); }
    size_t write(const uint8_t *source, size_t count) { return file_.write(source, count); }
    bool sync() { return file_.sync(); }
    int attrib() const { return file_.attrib(); }
    bool attrib(uint8_t attributes) { return file_.attrib(attributes); }
    void close() { file_.close(); }

private:
    File file_;
};

class SdFat
{
public:
    File open(const char *path, uint8_t flags)
    {
        const std::string key(path == nullptr ? "" : path);
        auto found = files_.find(key);
        if (found != files_.end() && found->second->readOnly &&
            (flags & O_WRONLY) != 0)
            return File();
        if ((flags & O_CREAT) != 0)
        {
            if (failOpen_)
                return File();
            auto entry = found == files_.end()
                             ? std::make_shared<HostFileEntry>()
                             : found->second;
            if ((flags & O_TRUNC) != 0)
                entry->bytes.clear();
            files_[key] = entry;
            return File(entry, writeLimit_, syncOk_);
        }
        return found == files_.end() ? File() : File(found->second);
    }
    bool exists(const char *path) const
    {
        return files_.find(path == nullptr ? "" : path) != files_.end();
    }
    bool remove(const char *path)
    {
        const std::string key(path == nullptr ? "" : path);
        auto found = files_.find(key);
        if (found == files_.end() || found->second->readOnly)
            return false;
        files_.erase(found);
        return true;
    }
    void seedReadOnly(const char *path, const std::vector<uint8_t> &bytes)
    {
        auto entry = std::make_shared<HostFileEntry>();
        entry->bytes = bytes;
        entry->readOnly = true;
        files_[path] = entry;
    }
    uint8_t sdErrorCode() const { return errorCode_; }
    uint8_t sdErrorData() const { return errorData_; }
    void failOpen(uint8_t code, uint8_t data)
    {
        failOpen_ = true;
        errorCode_ = code;
        errorData_ = data;
    }
    void limitWrite(size_t limit, uint8_t code, uint8_t data)
    {
        writeLimit_ = limit;
        errorCode_ = code;
        errorData_ = data;
    }
    void failSync(uint8_t code, uint8_t data)
    {
        syncOk_ = false;
        errorCode_ = code;
        errorData_ = data;
    }

private:
    std::map<std::string, std::shared_ptr<HostFileEntry>> files_;
    bool failOpen_ = false;
    size_t writeLimit_ = static_cast<size_t>(-1);
    bool syncOk_ = true;
    uint8_t errorCode_ = 0;
    uint8_t errorData_ = 0;
};

inline bool SdBaseFile::open(SdFat *sd, const char *path, uint8_t flags)
{
    if (sd == nullptr)
        return false;
    file_ = sd->open(path, flags);
    return static_cast<bool>(file_);
}

#endif
