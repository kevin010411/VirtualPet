#include "test_cases.h"

#include <assert.h>
#include <stdio.h>
#include <vector>

#include "appearance/SdAppearanceLoader.h"
#include "common/Crc32.h"
#include "common/FirmwareRandom.h"
#include "controller/Game.h"
#include "display/Renderer.h"
#include "game/CaptureGame.h"
#include "pet/Pet.h"
#include "pet/PetStorage.h"
#include "resources/AssetResources.h"
#include "resources/RuntimeContractLoader.h"
#include "resources/RuntimeTableBehavior.h"
#include "resources/detail/RuntimeTableFile.h"

static std::vector<uint8_t> captureLifetimeBundle(uint8_t targetCount)
{
    constexpr size_t headerSize = 40, targetSize = 18;
    const size_t sprites = headerSize + targetCount * targetSize + 2 * 128 * 160 + 2;
    std::vector<uint8_t> bundle(sprites + 2 * targetCount);
    const auto write16 = [&](size_t offset, uint16_t value) {
        bundle[offset] = value & 0xff;
        bundle[offset + 1] = value >> 8;
    };
    memcpy(bundle.data(), "VPCG", 4);
    bundle[4] = 1;
    bundle[6] = 1; // Density
    bundle[7] = targetCount;
    bundle[8] = 2; // Swim right
    bundle[10] = 64;
    bundle[11] = 20;
    bundle[12] = 120;
    bundle[13] = bundle[14] = 1;
    write16(16, 20);
    write16(18, 810);
    for (size_t slot = 0; slot < targetCount; ++slot)
    {
        const size_t record = headerSize + slot * targetSize;
        bundle[record] = slot == 0 ? 10 : 64;
        bundle[record + 1] = slot == 0 ? 40 : 80;
        bundle[record + 2] = bundle[record + 3] = 4;
        bundle[record + 4] = 5;
        bundle[record + 5] = bundle[record + 6] = 10;
        bundle[record + 7] = bundle[record + 8] = bundle[record + 9] = 1;
        write16(record + 12, slot + 1);
        write16(record + 14, sprites + 2 * slot);
        write16(sprites + 2 * slot, slot == 0 ? 0xf800 : 0x07e0);
    }
    return bundle;
}

void verifyCaptureFishLifetime()
{
    constexpr uint16_t red = 0xf800, green = 0x07e0;
    struct Outcomes : CaptureOutcomeHost
    {
        int32_t score = 0;
        bool onCaptureOutcome(CaptureAssets &, const CaptureAssets::Target *,
                              const CollectionRoundStats &, int32_t points, CatchResult &) override
        {
            score = points;
            return true;
        }
    } outcomes;
    SdFat sd;
    Adafruit_ST7735 display;
    AssetResources resources(&sd);
    Renderer renderer(&display, resources);
    CaptureGame game;
    const auto start = [&](uint32_t seed, uint8_t targetCount = 2) {
        sd.seedReadOnly("/capture.data", captureLifetimeBundle(targetCount));
        hostMillis = 0;
        outcomes.score = 0;
        FirmwareRandom::seed(seed);
        assert(game.start(renderer, resources, outcomes));
    };
    const auto advance = [&](unsigned long now) {
        hostMillis = now;
        game.update(false);
    };
    const auto visible = [&](uint16_t color) {
        return std::find(display.screen.begin(), display.screen.end(), color) != display.screen.end();
    };

    start(2);
    advance(4799);
    game.confirm();
    advance(4999);
    assert(visible(red) && !visible(green));
    advance(5000);
    assert(visible(green) && !visible(red) && outcomes.score == 0);
    advance(6100);
    assert(outcomes.score == 2); // The new fish can be caught by an already moving hook.

    start(1);
    advance(4400);
    game.confirm();
    advance(5000);
    assert(visible(green) && !visible(red) && outcomes.score == 0);
    advance(5600);
    assert(outcomes.score == 2); // A fish hooked before expiry stays on the line.

    start(2, 1);
    advance(5000);
    assert(game.clockRunning() && visible(red) && !visible(green) && outcomes.score == 0);
    puts("[PASS] Capture fish lifetime, replacement and catches at the expiry boundary");
}

void verifyCapture(const std::filesystem::path &root, bool shouldPause)
{
    constexpr uint16_t cardBackground = 0x1168;
    constexpr uint16_t scoreBackground = 0x11e9;
    SdFat sd;
    mountBundle(sd, root);
    AssetData::RuntimeManifest manifest = {};
    RuntimeTableInternal::RuntimeTableFile tableFile;
    assert(loadRuntimeManifest(&sd, manifest) && tableFile.open(&sd, manifest));
    const auto *items = tableFile.table().find(RuntimeTableInternal::CollectionItems);
    assert(items && (items->count == 1 || items->count == 4));
    const bool reverseDependencies = items->count == 4;
    const bool hasUnlockOrder = tableFile.table().find(RuntimeTableInternal::CollectionUnlockOrder) != nullptr;
    const uint32_t dependentOffset = items->offset + items->recordSize;
    Adafruit_ST7735 display;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(&display, resources);
    SdAppearanceLoader loader(&sd);
    Game game(pet, storage, renderer, resources, loader);
    assert(game.setup_game());
    tick(game, 1000);
    pet.setStageDays(10);
    assert(game.saveNow());
    const uint32_t before = storage.lastSaveSequence();

    const auto start = [&]
    {
        FirmwareRandom::seed(1);
        game.OnConfirmKey();
        assert(game.isCaptureActive());
    };
    const auto catchFish = [&]
    {
        game.OnConfirmKey();
        tick(game, hostMillis + 850);
    };
    const auto readCard = [&]
    {
        unsigned pages = 0;
        while (display.screen.front() == cardBackground && pages < 16)
        {
            game.OnConfirmKey();
            ++pages;
        }
        assert(pages > 1 && pages < 16);
    };
    const auto assertUnchanged = [&]
    {
        assert(pet.stageDays() == 10 && pet.persistentState().captureSeenMask == 0 && !pet.isCollectionOwned(0));
        assert(storage.lastSaveSequence() == before);
        if (reverseDependencies)
            for (uint8_t slot = 1; slot <= 3; ++slot) assert(!pet.isCollectionOwned(slot));
    };

    CaptureAssets capture;
    assert(capture.open(&sd));
    const auto &scene = capture.scene();
    capture.close();
    const auto timerPixel = [&](int x, int y) {
        return display.screen[(scene.timerY + y) * CaptureAssets::screenWidth + scene.timerX + x];
    };
    start();
    assert(timerPixel(2, 0) == 0x2aec && timerPixel(2, 12) == 0x1148);
    const auto initial = display.screen;
    tick(game, hostMillis + 1000);
    bool timerChanged = false;
    for (int y = 2; y < 11; ++y)
        for (int x = 8; x < 24; ++x)
            timerChanged |= timerPixel(x, y) != initial[(scene.timerY + y) * CaptureAssets::screenWidth + scene.timerX + x];
    assert(timerChanged && timerPixel(2, 12) == 0x1148);
    const auto readsBeforeCatch = sd.readsAt("/runtime.bin", dependentOffset);
    catchFish();
    if (reverseDependencies)
    {
        const auto reads = sd.readsAt("/runtime.bin", dependentOffset) - readsBeforeCatch;
        // The first dependent item is read once to acquire it and once to grant its appearance.
        assert(hasUnlockOrder ? reads == 2 : reads > 2);
    }
    assert(display.screen.front() == cardBackground);
    assertUnchanged();
    game.OnCaptureCancel();
    tick(game, hostMillis + 1);
    assert(!game.isCaptureActive());
    assertUnchanged();

    start();
    catchFish();
    assert(display.screen.front() == cardBackground);
    tick(game, hostMillis + 6000);
    assert(display.screen.front() == cardBackground);
    readCard();
    if (shouldPause)
    {
        tick(game, hostMillis + 1000);
        assert(display.screen.front() != scoreBackground);
    }
    hostMillis += shouldPause ? 4000 : 1;
    game.loop_game(true); // A center hold already in progress crosses the deadline.
    assert(display.screen.front() != scoreBackground);
    assertUnchanged();
    game.OnCaptureCancel();
    tick(game, hostMillis + 1);
    assertUnchanged();

    start();
    catchFish();
    assert(display.screen.front() == cardBackground);
    readCard();
    assertUnchanged();
    tick(game, hostMillis + 5001);
    assert(display.screen.front() == scoreBackground);
    const auto pixel = [&](int x, int y) { return display.screen[y * 128 + x]; };
    assert(pixel(50, 64) == 0xffff && pixel(70, 64) == 0xffff); // Top edges of the centered 4x score "10".
    assert(pixel(49, 64) == scoreBackground && pixel(53, 67) == 0xffff);
    assert(pixel(45, 139) == 0xffff && pixel(45, 138) == scoreBackground); // First stroke of 中鍵返回.
    assert(pet.stageDays() == 12 && pet.persistentState().captureSeenMask == 1 && pet.isCollectionOwned(0));
    assert(storage.lastSaveSequence() == before + 1);

    Pet restored;
    PetStorage rebootStorage(&sd);
    Game reboot(restored, rebootStorage, renderer, resources, loader);
    assert(reboot.setup_game() && restored.stageDays() == 12 && restored.isCollectionOwned(0));
    tick(reboot, hostMillis + 1000);
    FirmwareRandom::seed(1);
    reboot.OnConfirmKey();
    reboot.OnConfirmKey();
    tick(reboot, hostMillis + 850);
    assert(display.screen.front() != cardBackground);
    sd.limitWrite(12, 0, 0);
    tick(reboot, hostMillis + 5001);
    assert(restored.stageDays() == 12 && restored.persistentState().captureSeenMask == 1);
    Pet recovered;
    PetStorage recovery(&sd);
    assert(recovery.load(recovered, pet.persistentState().schemaFingerprint) && recovered.stageDays() == 12);
    if (reverseDependencies)
    {
        for (uint8_t slot = 1; slot <= 3; ++slot)
            assert(pet.isCollectionOwned(slot) && restored.isCollectionOwned(slot) && recovered.isCollectionOwned(slot));
        puts(hasUnlockOrder ? "[PASS] Reverse collection chain settles in one pass" :
                               "[PASS] Reverse collection chain settles from a legacy bundle");
    }
    puts("[PASS] Capture bundle: countdown, card paging and clock policy, cancellation before settlement, committed first catch after reboot, and failed-write rollback");
}

void verifyPreCaptureSave()
{
    SdFat sd;
    Pet old;
    old.setSchemaFingerprint(123);
    old.setStageDays(42);
    old.configureCollection(456);
    old.acquireCollection(63);
    PersistedPetState state = old.persistentState();
    state.version = Pet::kPetStatePreCaptureVersion;
    const auto *bytes = reinterpret_cast<const uint8_t *>(&state);
    constexpr size_t oldCrcAt = offsetof(PersistedPetState, captureTargetFingerprint);
    state.captureTargetFingerprint = Integrity::crc32(bytes, oldCrcAt);
    constexpr size_t oldSize = ENABLE_SHOP ? 560 : 400;
    sd.seedReadOnly("/state_a.bin", std::vector<uint8_t>(bytes, bytes + oldSize));
    Pet restored;
    PetStorage storage(&sd);
    assert(storage.load(restored, 123));
    assert(restored.stageDays() == 42 && restored.isCollectionOwned(63));
    assert(restored.persistentState().captureSeenMask == 0);
    puts("[PASS] Pre-capture saves retain pet values and collections");
}
