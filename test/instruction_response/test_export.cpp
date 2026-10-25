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
    std::ifstream input(argv[1],std::ios::binary);assert(input);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)),{});
    Source source={&bytes,memoryRead,static_cast<uint32_t>(bytes.size())};
    AssetData::RuntimeManifest manifest={};assert(readRuntimeManifest(source,manifest));
    PetBehaviorConfig config={};
    assert(parseRuntimeTableBehavior(bytes.data(),bytes.size(),manifest,1,1,config));
    assert(readU16(bytes.data()+4)==26);
    const bool maximum=strcmp(argv[2],"maximum")==0;
    const bool weighted=strcmp(argv[2],"weighted")==0, minimum=strcmp(argv[2],"minimum")==0;
    const InstructionResponseSettings expected = maximum ?
        InstructionResponseSettings{20,10000,10000,{65535,65535,65535}} : weighted ?
        InstructionResponseSettings{20,10000,10000,{0,65535,65535}} : minimum ?
        InstructionResponseSettings{1,500,100,{65535,0,0}} : InstructionResponseSettings{};
    const auto &settings=config.instructionSettings;
    const bool noGo=maximum || strcmp(argv[2],"nogo")==0;
    assert(settings.hard==(noGo || strcmp(argv[2],"hard")==0) && settings.noGoEnabled==noGo);
    assert(settings.typeWeights[0]==(maximum?65535:noGo?0:1) && settings.typeWeights[1]==(noGo?65535:0));
    assert(settings.roundCount==expected.roundCount && settings.answerMs==expected.answerMs &&
           settings.feedbackMs==expected.feedbackMs);
    for(uint8_t item=0;item<3;++item) assert(settings.targetWeights[item]==expected.targetWeights[item]);
    if (maximum) { assert(config.instructionEffectCount==24); }
    else if (strcmp(argv[2],"effects")==0) {
        assert(config.instructionEffectCount==2);
        const auto &correct=config.instructionEffects[0];
        const auto &wrong=config.instructionEffects[1];
        assert(correct.group==0 && wrong.group==1 && correct.statSlot==wrong.statSlot);
        assert(correct.value==5 && wrong.value==-5);
        PetBehaviorStatValues values={};
        for(uint8_t slot=0;slot<kPetBehaviorSlotCount;++slot) values.values[slot]=config.stats[slot].initialValue;
        assert(values.values[correct.statSlot]==9 && config.stats[correct.statSlot].maxValue==10);
        assert(applyPetBehaviorInstructionResults(config,1,2,values));
        assert(values.values[correct.statSlot]==5);
    } else assert(config.instructionEffectCount==0);
    assert(config.buttons[0].systemCommandId==RuntimeSystemCommandId::InstructionResponse);
    for (uint8_t i=0;i<3;++i) {
        assert(config.instructionOptions[i].valid() && config.instructionOptions[i].shared());
        assert(config.systemAnimations[35+i].valid());
        for (uint8_t j=0;j<i;++j) assert(config.instructionOptions[i].animationId!=config.instructionOptions[j].animationId);
    }
    assert(config.systemAnimations[38].valid()==(weighted||noGo) && config.systemAnimations[39].valid()==(weighted||noGo));
    for(uint8_t role=40;role<43;++role) assert(config.systemAnimations[role].valid()==noGo);
    InstructionResponse game;
    game.start(settings,[](uint32_t bound)->uint32_t {return bound-1;});
    uint32_t now=0;
    for(uint8_t round=0;round<settings.roundCount;++round) {
        assert(game.phase()==InstructionResponse::Phase::Preparing);
        assert(game.targetItem()==(minimum?0:2));
        assert(game.isNoGo()==noGo);
        game.ready(now,0);
        for(uint8_t position=0;position<3;++position)
            if((!noGo || (maximum && round%2)) && game.itemAt(position)==game.targetItem()) game.input(1<<position,1<<position,now+settings.answerMs-1);
        game.update(now+settings.answerMs-1);assert(game.phase()==InstructionResponse::Phase::Answer);
        now+=settings.answerMs;game.update(now);assert(game.phase()==InstructionResponse::Phase::Feedback && game.correct()==!(maximum && round%2));
        now+=700;game.ready(now,0);
        game.update(now+settings.feedbackMs-1);assert(game.phase()==InstructionResponse::Phase::Feedback);
        now+=settings.feedbackMs;game.update(now);
    }
    assert(game.phase()==InstructionResponse::Phase::Finished && game.completedRounds()==settings.roundCount);
    uint32_t results=0; uint8_t count=0;
    assert(game.consumeCompletion(results,count) && count==settings.roundCount);
    assert(!game.consumeCompletion(results,count));
    if(maximum) {
        assert(results==0x55555);
        PetBehaviorStatValues values={};
        for(uint8_t slot=0;slot<kPetBehaviorSlotCount;++slot) values.values[slot]=config.stats[slot].initialValue;
        assert(applyPetBehaviorInstructionResults(config,results,count,values));
        for(uint8_t slot=0;slot<kPetBehaviorSlotCount;++slot) {
            const uint32_t index=config.stats[slot].initialValue-9;
            assert(index<12 && values.values[slot]==UINT32_MAX-(index+5)-(index%2?index:0));
        }
        puts("[PASS] maximum 20 rounds/24 effects/12 distinct Stat identities: ordered changes, sets, per-step clamps and single consumption");
    }
    puts("[PASS] actual Web v26 save/upload/export: settings/weights/roles decoded and drive configured game through full final feedback; option references remain distinct");
}
