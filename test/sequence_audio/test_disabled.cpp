#include <assert.h>
#include <fstream>
#include <iterator>
#include <vector>
#include <string.h>
#include <stdio.h>
#include "resources/RuntimeTableBehavior.h"
#include "resources/detail/RuntimeTableReader.h"

int main(int argc, char **argv)
{
    assert(argc == 3);
    for (int argument = 1; argument <= 2; ++argument)
    {
        std::ifstream file(argv[argument], std::ios::binary);
        assert(file);
        const std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
        AssetData::RuntimeManifest manifest = {};
        memcpy(manifest.bundleId.bytes, bytes.data() + 28, 16);
        manifest.fileSize = bytes.size();
        manifest.schemaFingerprint = RuntimeTableInternal::readU32(bytes.data() + 44);
        manifest.fileCrc32 = RuntimeTableInternal::readU32(bytes.data() + 48);
        PetBehaviorConfig config = {};
        const bool loaded = parseRuntimeTableBehavior(bytes.data(), bytes.size(), manifest, 1, 1, config);
        assert(loaded == (argument == 1)); // Silent v21 loads; animation audio requires the player.
    }
    puts("[PASS] ENABLE_SEQUENCE_AUDIO=0 loads silent v21 and refuses animation audio");
}
