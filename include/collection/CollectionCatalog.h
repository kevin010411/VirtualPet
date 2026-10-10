#pragma once
#include "pet/PetBehaviorTypes.h"
#include "resources/detail/RuntimeTableReader.h"

class Pet;
class Renderer;

// Runtime records stay on SD. Only the active page/focus and a bounded record
// buffer live in RAM; no framebuffer or array of 64 resolved image references.
bool validateCollectionCatalog(const RuntimeTableInternal::RuntimeTable &table,
                               uint8_t statCount, PetBehaviorConfig &config);
bool validateCollectionProducts(const RuntimeTableInternal::RuntimeTable &table, BundleReader &reader);
bool evaluateCollectionCatalog(SdFat *sd, const PetBehaviorConfig &config, Pet &pet,
                               bool &changed);
bool applyCollectionOutfitGrants(SdFat *sd, const PetBehaviorConfig &config, Pet &pet);

class CollectionCatalog
{
public:
    CollectionCatalog(Pet &pet, Renderer &renderer, const PetBehaviorConfig &config);
    bool start();
    bool active() const { return opened; }
    bool move(bool right);
    bool confirm();
    bool redraw();
    bool takeActivation(uint8_t &species, uint8_t &outfit);
    void exit() { opened = false; detail = false; }
private:
    Pet &pet;
    Renderer &renderer;
    const PetBehaviorConfig &config;
    uint8_t count = 0;
    uint8_t focus = 0;
    uint8_t page = 0;
    bool opened = false;
    bool detail = false;
    uint8_t targetSpecies = 0;
    uint8_t targetOutfit = 0;
    bool drawCell(const RuntimeTableInternal::RuntimeTable &table, uint8_t cell, bool selected);
};
