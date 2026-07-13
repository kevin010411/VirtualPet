#ifndef ASSET_MANIFEST_H
#define ASSET_MANIFEST_H

#include <Arduino.h>
#include <SdFat.h>
#include "animation/domain/Animation.h"

struct AnimationMeta
{
    static constexpr uint8_t kMaxPathLength = 47;

    char path[kMaxPathLength + 1];
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
    static constexpr uint8_t kMaxAnimationVariants = 16;
    static constexpr uint8_t kMaxAnimationNameLength = 15;

    void reset();
    bool load(SdFat *sd, const char *speciesCode, const char *outfitCode);
    bool hasPathError() const;
    bool hasCapacityError() const;

    const AnimationMeta *metaFor(AnimationId id) const;
    AnimationMeta *metaForName(const char *name);
    const AnimationMeta *metaForName(const char *name) const;
    uint8_t variantCountFor(const char *baseName) const;
    const char *variantNameFor(const char *baseName, uint8_t index) const;

    // These are used by the manifest parser. They retain fixed storage and never allocate.
    bool registerNamedAnimation(const char *name, const AnimationMeta &meta);
    bool registerVariantAnimation(const char *baseName, const char *name, const AnimationMeta &meta);
};

#endif
