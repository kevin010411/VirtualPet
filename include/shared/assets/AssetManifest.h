#ifndef ASSET_MANIFEST_H
#define ASSET_MANIFEST_H

#include <Arduino.h>
#include <SdFat.h>
#include "animation/domain/Animation.h"

enum class AssetFormat : uint8_t
{
    BmpSequence,
    RleRgb565Sequence
};

struct AnimationMeta
{
    char path[64];
    AssetFormat format;
    uint16_t width;
    uint16_t height;
    uint16_t frameCount;
    uint16_t frameIntervalMs;
    bool configured;
    bool singleFile;
};

class AssetManifest
{
public:
    static constexpr uint8_t kMaxNamedAnimations = 4;
    static constexpr uint8_t kMaxAnimationNameLength = 15;

    void reset();
    bool load(SdFat *sd, const char *speciesCode, const char *outfitCode);

    AnimationMeta *metaFor(AnimationId id);
    const AnimationMeta *metaFor(AnimationId id) const;
    AnimationMeta *metaForName(const char *name);
    const AnimationMeta *metaForName(const char *name) const;
};

#endif
