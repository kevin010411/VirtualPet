#include <assert.h>
#include <cstring>
#include <fstream>
#include <iterator>
#include <vector>
#include <stdio.h>
#include "resources/RuntimeTableBehavior.h"
#include "display/FrameDecoder.h"
int main(int argc,char **argv)
{
    assert(argc==3);
    const bool maximum=strcmp(argv[2],"maximum")==0;
    CustomLayoutHost::root=argv[1];
    SdFat sd;
    AssetData::RuntimeManifest manifest={};
    assert(loadRuntimeManifest(&sd,manifest));
    uint8_t scratch[AssetData::kIoScratchBytes]={};
    BundleReader reader(&sd,scratch,sizeof(scratch));
    assert(reader.configureBundle(manifest.bundleId));
    PetBehaviorConfig config={};
    assert(loadCompleteRuntimeTable(&sd,manifest,reader,1,1,config));
    Adafruit_ST7735 display;
    uint8_t readBuffer[FrameDecoder::kDataReadBufferBytes]={};
    uint16_t lines[FrameDecoder::kLineBufferPixels]={};
    for (uint8_t item=0;item<3;++item)
    {
        const auto &ref=config.instructionOptions[item];
        AssetData::AssetFrameAddress address={ref.speciesSlot,ref.outfitSlot,ref.animationId,0,0};
        assert(FrameDecoder::showDataFrame(reader,address,&display,readBuffer,sizeof(readBuffer),lines,
                FrameDecoder::kLineBufferPixels,item*43,128,12,42,32));
        for (unsigned y=128;y<160;++y)
            for (unsigned x=item*43;x<item*43+42;++x)
                assert(display.screen[y*128+x]!=0xdead);
    }
    for (unsigned y=128;y<160;++y)
        assert(display.screen[y*128+42]==0xdead && display.screen[y*128+85]==0xdead);
    for (unsigned pixel=0;pixel<128*128;++pixel) assert(display.screen[pixel]==0xdead);
    if(maximum) {
        const uint16_t intervals[]={50,10000,167,500};
        for(uint8_t role=35;role<43;++role) for(uint8_t version=0;version<4;++version) {
            const auto &ref=config.systemAnimations[role];
            AssetData::AssetFrameAddress address={ref.speciesSlot,ref.outfitSlot,ref.animationId,version,0};
            AssetData::AnimationRecord animation;
            assert(reader.resolveAnimation(address,animation));
            assert(animation.frameCount==2 && animation.frameMs==intervals[version]);
            const auto options=display.screen;
            for(uint8_t frame=0;frame<2;++frame) {
                address.frameIndex=frame;
                assert(FrameDecoder::showDataFrame(reader,address,&display,readBuffer,sizeof(readBuffer),lines,
                    FrameDecoder::kLineBufferPixels,0,32,12,128,96));
                for(unsigned pixel=0;pixel<128*32;++pixel) assert(display.screen[pixel]==0xdead);
                for(unsigned pixel=128*128;pixel<128*160;++pixel) assert(display.screen[pixel]==options[pixel]);
            }
        }
        puts("[PASS] all eight roles x four versions x two frames decode with authored intervals and bounded pixels");
        return 0;
    }
    for (uint8_t role=38;role<=39;++role)
    {
        const auto &ref=config.systemAnimations[role];
        if (!ref.valid()) continue;
        AssetData::AssetFrameAddress address={ref.speciesSlot,ref.outfitSlot,ref.animationId,0,0};
        AssetData::AnimationRecord animation;
        assert(reader.resolveAnimation(address,animation));
        assert(animation.frameCount==(role==38?1:2));
        assert(animation.frameMs==(role==38?10000:50));
        const auto options=display.screen;
        for(uint16_t frame=0;frame<animation.frameCount;++frame)
        {
            address.frameIndex=frame;
            assert(FrameDecoder::showDataFrame(reader,address,&display,readBuffer,sizeof(readBuffer),lines,
                    FrameDecoder::kLineBufferPixels,0,32,12,128,96));
            for(unsigned pixel=0;pixel<128*32;++pixel) assert(display.screen[pixel]==0xdead);
            for(unsigned pixel=128*128;pixel<128*160;++pixel) assert(display.screen[pixel]==options[pixel]);
        }
    }
    for(uint8_t role=40;role<43;++role)
    {
        const auto &ref=config.systemAnimations[role];
        if(!ref.valid()) continue;
        AssetData::AssetFrameAddress address={ref.speciesSlot,ref.outfitSlot,ref.animationId,0,0};
        const auto options=display.screen;
        assert(FrameDecoder::showDataFrame(reader,address,&display,readBuffer,sizeof(readBuffer),lines,
                FrameDecoder::kLineBufferPixels,0,32,12,128,96));
        for(unsigned pixel=0;pixel<128*32;++pixel) assert(display.screen[pixel]==0xdead);
        for(unsigned pixel=128*128;pixel<128*160;++pixel) assert(display.screen[pixel]==options[pixel]);
    }
    puts("[PASS] actual exported 42x32 option frames decoded into TFT capture at x0/43/86, exact 1px gaps, bounded scanline writes");
    if(config.systemAnimations[38].valid())
        puts("[PASS] actual single/multi-frame feedback decodes inside 128x96 without changing top or option pixels");
}
