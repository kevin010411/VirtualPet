#include <assert.h>
#include <algorithm>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>

#include "controller/StatusSetContract.h"
#include "controller/SystemCommandCatalog.h"
#include "appearance/RuntimeTableAppearance.h"
#include "appearance/SdAppearanceLoader.h"
#include "pet/PetBehaviorRuntimeRules.h"
#include "resources/RuntimeTableBehavior.h"
#include "game/InstructionResponse.h"

namespace AssetData
{
bool sameBundleId(const BundleId &left, const BundleId &right)
{
    for (uint8_t index = 0; index < sizeof(left.bytes); ++index)
        if (left.bytes[index] != right.bytes[index])
            return false;
    return true;
}

// Behavior fixtures do not mount asset packs. The complete SD-loader tests own
// physical pack resolution; this seam only makes the behavior reader linkable.
bool animationReferenceExists(BundleReader &, const AnimationRef &, uint8_t)
{
    return true;
}
} // namespace AssetData

BundleReader::BundleReader(SdFat *sd, uint8_t *scratch, size_t scratchSize)
    : sd_(sd), scratch_(scratch), scratchSize_(scratchSize)
{
}

bool BundleReader::configureBundle(const AssetData::BundleId &bundleId)
{
    bundleId_ = bundleId;
    bundleConfigured_ = true;
    return true;
}

const char *BundleReader::firstErrorResource() const
{
    return firstErrorResource_;
}

#include "resources/detail/RuntimeTableReader.h"
#include <stdio.h>
using namespace RuntimeTableInternal;
static bool memoryRead(void *context, uint32_t offset, uint8_t *output, size_t size)
{
    const auto &bytes=*static_cast<std::vector<uint8_t> *>(context);
    if (offset+size>bytes.size()) return false;
    memcpy(output,bytes.data()+offset,size);return true;
}
int main(int argc,char **argv)
{
    assert(argc==3);
    for(int card=1;card<=2;++card) {
        std::ifstream input(argv[card],std::ios::binary);assert(input);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
        assert(bytes.size() >= 64 && bytes[4] == 26 && bytes[5] == 0);
        for (uint8_t version : {22, 23, 24, 25, 27})
        {
            auto incompatible = bytes;
            incompatible[4] = version;
            Source rejected = {&incompatible, memoryRead, static_cast<uint32_t>(incompatible.size())};
            AssetData::RuntimeManifest rejectedManifest = {};
            assert(!readRuntimeManifest(rejected, rejectedManifest));
        }
        Source source={&bytes,memoryRead,static_cast<uint32_t>(bytes.size())};
        AssetData::RuntimeManifest manifest={};assert(readRuntimeManifest(source,manifest));
        PetBehaviorConfig config={};
        const bool accepted=parseRuntimeTableBehavior(bytes.data(),bytes.size(),manifest,1,1,config);
        assert(accepted==(card==1));
        if(accepted) assert(config.buttons[0].systemCommandId!=RuntimeSystemCommandId::InstructionResponse);
    }
    puts("[PASS] feature-disabled reader accepts actual unused v26 export and rejects enabled maximum package");
    puts("[PASS] v22/v23/v24/v25 and unknown v27 headers rejected; no legacy compatibility parser");
}
