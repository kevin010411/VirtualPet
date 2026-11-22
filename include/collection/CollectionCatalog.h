#pragma once
#include "display/DisplayRect.h"
#include "resources/RuntimeTableConfig.h"
#include "resources/detail/RuntimeTableReader.h"

class Pet;
class Renderer;
class AssetResources;

// Runtime records stay on SD. Only the active page/focus and a bounded record
// buffer live in RAM; no framebuffer or array of 64 resolved image references.
bool loadCollectionCatalog(const RuntimeTableInternal::RuntimeTable &table,
                           CollectionSettings &config, uint8_t (&trackedActions)[8], uint8_t &trackedActionCount);
bool evaluateCollectionCatalog(SdFat *sd, const RuntimeTableConfig &config, Pet &pet,
                               bool &changed);
bool applyCollectionOutfitGrants(SdFat *sd, const RuntimeTableConfig &config, Pet &pet);

class CollectionCatalog
{
public:
    CollectionCatalog(Pet &pet, Renderer &renderer, AssetResources &resources, const RuntimeTableConfig &config);
    bool start();
    bool active() const { return opened; }
    bool move(bool right);
    bool confirm();
    bool redraw();
    bool takeActivation(uint8_t &species, uint8_t &outfit);
    bool takeReplay(AssetData::AnimationRef &animation, uint8_t &version);
    void exit() { opened = false; detail = false; replayAnimation = {}; }
private:
    Pet &pet;
    Renderer &renderer;
    AssetResources &resources;
    const RuntimeTableConfig &config;
    uint8_t count = 0;
    uint8_t focus = 0;
    uint8_t page = 0;
    bool opened = false;
    bool detail = false;
    bool cards = false;
    bool backgroundBands = false;
    uint8_t detailPage = 0;
    uint8_t targetSpecies = 0;
    uint8_t targetOutfit = 0;
    AssetData::AnimationRef replayAnimation = {};
    uint8_t replayVersion = 0;
    bool selectReplay(const RuntimeTableInternal::RuntimeTable &table, uint8_t slot);
    uint8_t lastCell() const;
    bool drawCell(const RuntimeTableInternal::RuntimeTable &table, uint8_t cell, bool selected,
                  const DisplayRect *crop = nullptr);
    bool drawDetail(const RuntimeTableInternal::RuntimeTable &table, uint8_t *item);
    bool restoreDetail(const RuntimeTableInternal::RuntimeTable &table);
    bool restoreBackground(const RuntimeTableInternal::RuntimeTable &table, const uint8_t *cfg,
                           DisplayRect rect);
};
