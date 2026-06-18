#ifndef ASSET_MANIFEST_H
#define ASSET_MANIFEST_H

#include <Arduino.h>
#include <SdFat.h>
#include "domain/Animation.h"

enum class AssetFormat : uint8_t
{
    BmpSequence,
    RleRgb565Sequence
};

struct AnimationMeta
{
    char path[96];
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
    void reset();
    bool load(SdFat *sd, const char *speciesCode, const char *outfitCode);

    AnimationMeta *metaFor(AnimationId id);
    const AnimationMeta *metaFor(AnimationId id) const;
};

#endif
