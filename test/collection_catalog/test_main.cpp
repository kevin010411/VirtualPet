#include "resources/AssetResources.h"
#include <assert.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <stdio.h>
#include <string.h>
#include "controller/Game.h"
#if ENABLE_PROJECT_MENUS || ENABLE_STAT_EVENTS
#include "controller/CommandController.h"
#endif
#include "appearance/SdAppearanceLoader.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetStorage.h"
#include "pet/PetBehaviorRuntime.h"
#include "animation/AnimationController.h"
#include "resources/RuntimeContractLoader.h"
#include "common/FirmwareRandom.h"
#include "collection/CollectionCatalog.h"
#include "appearance/RuntimeTableAppearance.h"
#if ENABLE_CAPTURE_GAME
#include "common/Crc32.h"
#endif

unsigned long hostMillis = 0;

static void tick(Game &game, unsigned long now)
{
    hostMillis = now;
#if COLLECTION_USE_RTC
    game.synchronizeTime(static_cast<uint32_t>(now / 1000));
#endif
    game.loop_game();
}

static void mountBundle(SdFat &sd, const std::filesystem::path &root)
{
    for (const auto &entry : std::filesystem::recursive_directory_iterator(root))
    {
        if (!entry.is_regular_file()) continue;
        std::ifstream input(entry.path(), std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const auto path = "/" + std::filesystem::relative(entry.path(), root).generic_string();
        sd.seedReadOnly(path.c_str(), bytes);
    }
}

#if ENABLE_CAPTURE_GAME
static void verifyCapture(const std::filesystem::path &root, bool shouldPause)
{
    constexpr uint16_t cardBackground = 0x1168;
    constexpr uint16_t scoreBackground = 0x11e9;
    SdFat sd;
    mountBundle(sd, root);
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
    catchFish();
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
    puts("[PASS] Capture bundle: countdown, card paging and clock policy, cancellation before settlement, committed first catch after reboot, and failed-write rollback");
}

static void verifyPreCaptureSave()
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
#endif

#if COLLECTION_USE_RTC
static void verifyClassicGuess(const std::filesystem::path &root)
{
    for (bool interrupted : {false, true})
    {
        hostMillis = 0;
        SdFat sd; mountBundle(sd, root);
        Adafruit_ST7735 display; Pet pet; PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(&display, resources); SdAppearanceLoader loader(&sd);
        Game game(pet, storage, renderer, resources, loader);
        game.synchronizeTime(0);
        assert(game.setup_game());
        game.OnRightKey(); FirmwareRandom::seed(1); game.OnConfirmKey();
        tick(game, 51);
        game.OnRightKey(); // Round result plays for five seconds; final is deferred.
        tick(game, 52); // Start the queued result before advancing the RTC snapshot.
        assert(pet.customStat(0) == 10 && pet.isCollectionOwned(0) && !pet.isCollectionOwned(1));
        assert(pet.isOutfitUnlocked(3) && !pet.isOutfitUnlocked(4));
        const uint32_t roundSequence = storage.lastSaveSequence();
        tick(game, 8052); // Four RTC days become pending while result playback finishes.
        assert(game.hasPendingTime() && pet.stageDays() == 0);
        assert(storage.lastSaveSequence() == roundSequence);
        if (interrupted) sd.limitWrite(12, 0, 0);
        // Classic mode queues both item and correctness result animations.
        // Advance each queued duration while retaining overdue RTC days.
        for (unsigned int step = 0; step < 16 && pet.customStat(0) != 20; ++step)
            tick(game, 8052 + step * 8000);
        assert(game.hasPendingTime() && pet.stageDays() == 1);
        assert(pet.customStat(0) == 20 && pet.isCollectionOwned(1) && pet.isOutfitUnlocked(4));
        Pet recovered; PetStorage recovery(&sd);
        assert(recovery.load(recovered, pet.persistentState().schemaFingerprint));
        if (interrupted)
        {
            assert(recovered.customStat(0) == 10 && recovered.stageDays() == 0);
            assert(recovered.isCollectionOwned(0) && !recovered.isCollectionOwned(1));
            assert(recovered.isOutfitUnlocked(3) && !recovered.isOutfitUnlocked(4));
        }
        else
        {
            assert(storage.lastSaveSequence() == roundSequence + 1);
            assert(recovered.customStat(0) == 20 && recovered.stageDays() == 1);
            assert(recovered.isCollectionOwned(0) && recovered.isCollectionOwned(1));
            assert(recovered.isOutfitUnlocked(3) && recovered.isOutfitUnlocked(4));
        }
    }
    puts("[PASS] Classic Guess RTC batch: automatic final settlement commits acquisition and grants before catch-up drains; interrupted final write restores complete prior round snapshot");
}
#endif

static void verifyTransactions()
{
    // Every possible interrupted byte boundary of the actual v16 writer.
    for (size_t boundary = 0; boundary <= sizeof(PersistedPetState); ++boundary)
    {
        SdFat sd;
        Pet pet;
        pet.setSchemaFingerprint(123);
        pet.setCustomStat(0, 17); pet.setStageDays(9);
        pet.configureCollection(456); pet.acquireCollection(63);
        pet.configureTrackedActions(789); pet.incrementActionCount(7);
        pet.unlockOutfit(3);
        PetStorage storage(&sd);
        assert(storage.save(pet));
        Pet baseline; PetStorage baselineStorage(&sd);
        assert(baselineStorage.load(baseline, 123));
        const PersistedPetState committed = baseline.persistentState();
        pet.setSpeciesSlot(2); pet.setOutfitSlot(2);
        pet.setCustomStat(0, 88); pet.setStageDays(20); pet.acquireCollection(0);
        pet.incrementActionCount(0); pet.unlockOutfit(2);
        sd.limitWrite(boundary, 0, 0);
        if (boundary == sizeof(PersistedPetState)) sd.failSync(0, 0);
        assert(!storage.save(pet));
        Pet recovered; PetStorage recovery(&sd);
        assert(recovery.load(recovered, 123));
        assert(memcmp(&committed, &recovered.persistentState(), sizeof(committed)) == 0);
    }
    SdFat sd;
    Pet pet; pet.setSchemaFingerprint(123); pet.configureCollection(456);
    pet.configureTrackedActions(789); pet.acquireCollection(0);
    pet.setCustomStat(0, 17); pet.incrementActionCount(0);
    PetStorage storage(&sd); assert(storage.save(pet));
    Pet baseline; PetStorage baselineStorage(&sd); assert(baselineStorage.load(baseline, 123));
    const PersistedPetState committed = baseline.persistentState();
    pet.setCustomStat(0, 88); pet.acquireCollection(63); pet.incrementActionCount(0);
    assert(storage.save(pet));
    sd.corrupt("/state_b.bin", 30);
    Pet recovered; PetStorage recovery(&sd); assert(recovery.load(recovered, 123));
    assert(memcmp(&committed, &recovered.persistentState(), sizeof(committed)) == 0);
    sd.corrupt("/state_a.bin", 40);
    Pet rejected; PetStorage noValidSlot(&sd); assert(!noValidSlot.load(rejected, 123));
    assert(!sd.exists("/state_a.bin") && !sd.exists("/state_b.bin"));
    printf("[PASS] Production A/B writer: all %zu truncation boundaries, failed sync, CRC fallback and both-corrupt discard recover whole snapshots\n", sizeof(PersistedPetState) + 1);
}

static void verify64Grid(Game &game, Pet &pet, Adafruit_ST7735 &display, bool twoPages = false)
{
    const auto pixel = [&](int x, int y) { return display.screen[y * 128 + x]; };
    game.OnConfirmKey();
    assert(game.isCollectionCatalogActive());
    assert(pixel(1,1) == 0x07ff); // The 30px return image is inset by one pixel.
    const size_t before = display.windows.size();
    game.OnRightKey();
    assert(display.windows.size() == before + 2);
    assert(pixel(1,1) == 0xffe0 && pixel(32,0) == 0xffff);
    assert(pixel(48,16) == 0x001f); // Shared unknown image, opaque blue center.
    assert(pixel(36,4) == 0x0820); // Transparency composed against this cell's background.
    game.OnLeftKey();
    assert(pixel(32,0) == 0x0820); // Removing selection restores the original background.
    const size_t beforeShortPage = display.windows.size();
    game.OnLeftKey(); // First return wraps to item64, fifth page, cell4.
    bool selectedTileWritten = false;
    for (size_t i = beforeShortPage; i < display.windows.size(); ++i)
    {
        const auto &window = display.windows[i];
        assert(window.y + window.height <= 128);
        selectedTileWritten |= window.x == 0 && window.y == 32 && window.width == 32 && window.height == 32;
    }
    assert(selectedTileWritten);
    assert(pixel(0,32) == 0xffff && pixel(16,48) == 0x001f);
    assert(pixel(32,32) == 0x28a0); // Empty cell5 stays background, never an item.
    const auto fifthFocused = display.screen;
    size_t beforeDetail = display.windows.size();
    game.OnConfirmKey();
    assert(display.windows.size() == beforeDetail + 1 && display.windows.back().y == 112);
    assert(pixel(0,0) == fifthFocused[0] && pixel(0,112) == 0xf800 && pixel(127,159) == 0xf800);
    if (twoPages)
    {
        beforeDetail = display.windows.size();
        game.OnConfirmKey();
        assert(display.windows.size() == beforeDetail + 1 && display.windows.back().y == 112);
        assert(pixel(0,0) == fifthFocused[0] && pixel(0,112) == 0x07e0);
        game.redrawAllNow();
        assert(pixel(0,112) == 0x07e0);
        game.OnLeftKey();
        assert(pixel(0,112) == 0x07e0);
    }
    beforeDetail = display.windows.size();
    game.OnConfirmKey();
    assert(display.windows.size() > beforeDetail);
    for (size_t i = beforeDetail; i < display.windows.size(); ++i)
        assert(display.windows[i].y >= 112);
    assert(display.screen == fifthFocused); // Detail close restores exact page/focus.
    const size_t beforeWrap = display.windows.size();
    game.OnRightKey();
    assert(display.windows.size() == beforeWrap + 16); // Replace each grid tile once.
    assert(pixel(1,1) == 0x07ff); // Last item skips all empty cells and wraps.
    game.OnLeftKey();
    assert(display.screen == fifthFocused); // Vacated cells restore their own patterned background.
    game.OnRightKey();
    for (int step = 0; step < 13; ++step) game.OnRightKey();
    const auto fullPage = display.screen;
    game.OnConfirmKey();
    if (twoPages) game.OnConfirmKey();
    beforeDetail = display.windows.size();
    game.OnConfirmKey();
    for (size_t i = beforeDetail; i < display.windows.size(); ++i)
        assert(display.windows[i].y >= 112);
    assert(display.screen == fullPage); // Restore the lower half of all four occupied cells.
    for (int step = 0; step < 13; ++step) game.OnLeftKey();
    // Each page has its own return cell and every return exits the production caller.
    for (int page = 0; page < 5; ++page)
    {
        if (page != 0) game.OnConfirmKey();
        for (int step = 0; step < page * 16; ++step) game.OnRightKey();
        assert(pixel(1,1) == 0x07ff);
        const size_t beforeBack = display.windows.size();
        game.OnConfirmKey();
        assert(!game.isCollectionCatalogActive());
        tick(game, hostMillis); // Production redraw is requested and serviced by the loop.
        assert(display.windows.size() > beforeBack);
    }
    for (uint8_t slot = 0; slot < 64; ++slot) pet.acquireCollection(slot);
    game.OnConfirmKey();
    game.OnLeftKey();
    assert(pixel(16,48) == 0xf800); // Slot63 owned product uses final tile frame254/255.
    game.OnConfirmKey();
    assert(pixel(1,1) == 0xffe0 && pixel(0,112) == 0x07e0 && pixel(127,159) == 0x07e0); // Story descriptor63.
    if (twoPages)
    {
        game.OnConfirmKey();
        assert(pixel(1,1) == 0xffe0 && pixel(0,112) == 0xf800);
    }
    game.OnConfirmKey();
    assert(pixel(0,32) == 0xffff && pixel(16,48) == 0xf800);
    puts("[PASS] Real64 bundle: five pages, cyclic navigation, blank skipping, all returns, transparent per-cell composition, final tile/story frames, exact detail focus and two-cell redraw");
}

static void verifyCatchUpCollectionWindow(const std::filesystem::path &root)
{
    using namespace RuntimeTableInternal;
    SdFat sd;
    mountBundle(sd, root);
    PetBehaviorConfig config = {};
    assert(loadRuntimeContract(&sd, 1, 1, config));
    std::ifstream input(root / "runtime.bin", std::ios::binary);
    std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    auto put = [&](size_t offset, uint64_t value, uint8_t width) {
        for (uint8_t byte = 0; byte < width; ++byte) bytes[offset + byte] = value >> (8 * byte);
    };
    size_t collectionCondition = 0, item = 0;
    for (uint16_t index = 0; index < readU16(bytes.data() + 24); ++index)
    {
        const size_t entry = 64 + index * 16;
        const uint16_t type = readU16(bytes.data() + entry);
        const size_t offset = readU32(bytes.data() + entry + 4);
        if (type == CollectionItems) item = offset;
        if (type != CollectionConditions && type != EvolutionConditions && type != OutfitUnlockConditions) continue;
        if (type == CollectionConditions) collectionCondition = offset;
        for (uint16_t row = 0; row < readU16(bytes.data() + entry + 8); ++row)
        {
            const size_t record = offset + row * readU16(bytes.data() + entry + 10);
            put(record, type == CollectionConditions ? 1 : runtimeValueIdForPetStat(1), 2);
            put(record + 2, 0, 8);
            put(record + 10, 100, 8);
        }
    }
    assert(collectionCondition && item);
    put(collectionCondition, 0, 2);
    put(collectionCondition + 2, 95, 8);
    put(collectionCondition + 10, 95, 8);
    put(item + 1, 0, 1); // Image activation; no appearance grant.
    put(item + 2, 0, 2);
    put(item + 4, 1, 2);
    sd.seedReadOnly("/runtime.bin", bytes);
    config.stats[0] = {true, 100, 0, 100, -1};
    config.stats[1] = {true, 0, 0, 100, 0};
    config.petStateCount = 0;
    config.deathEnabled = false;
    PetBehaviorStatValues state = {};
    state.values[0] = 100;
    PetBehaviorDailyChangePauses pauses = {};
    Pet pet;
    uint32_t remaining = 100, batches = 0;
    while (remaining)
    {
        uint32_t days = 0;
        assert(runtimeTableCatchUpDays(&sd, config, state, pauses, remaining, days));
        if (batches == 0) assert(days == 5);
        applyPetBehaviorDailyChanges(config, state, pauses, days);
        assert(pet.commitPetDay(state.values, kPetBehaviorSlotCount, days));
        bool changed = false;
        assert(evaluateCollectionCatalog(&sd, config, pet, changed));
        if (batches == 0) assert(pet.isCollectionOwned(0) && pet.customStat(0) == 95);
        remaining -= days;
        ++batches;
    }
    assert(pet.isCollectionOwned(0) && pet.customStat(0) == 0 && batches == 3);
    puts("[PASS] Production SD catch-up acquires a one-day Collection window before skipping the remaining days");
}

#if ENABLE_SHOP
static int verifyShop(const std::filesystem::path &root)
{
    SdFat sd;
    mountBundle(sd, root);
    Adafruit_ST7735 display;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(&display, resources);
    SdAppearanceLoader loader(&sd);
    Game game(pet, storage, renderer, resources, loader);
    assert(game.setup_game());

    assert(pet.customStat(0) == 50);
    const auto initialSpecies = pet.speciesSlot();
    assert(pet.stageDays() == 0);
    game.OnConfirmKey(); // Shop command
    const auto browsing = display.screen;
    const auto matchesFullRedraw = [&] {
        const auto incremental = display.screen;
        game.redrawAllNow();
        assert(display.screen == incremental);
    };
    std::ifstream shopData(root / "shop.data", std::ios::binary);
    shopData.seekg(23);
    const int items = shopData.get(), products = shopData.get();
    const bool list = shopData.get() == 16;
    const int species = shopData.get();
    shopData.seekg(39 + (129 + 2 * species) * items + 70 * products + (list ? 0 : 4 * (items + products + 3)));
    char confirmationTag[4]{};
    shopData.read(confirmationTag, sizeof(confirmationTag));
    const bool configured = std::string(confirmationTag, sizeof(confirmationTag)) == "SCF1";
    size_t beforeMove = display.windows.size();
    game.OnRightKey(); // Product to inventory entry, with both rows in view in list mode.
    if (list)
    {
        assert(display.windows.size() == beforeMove + 2);
        for (size_t i = beforeMove; i < display.windows.size(); ++i)
            assert(display.windows[i].height == 16 && display.windows[i].width == 120);
    }
    else
    {
        assert(display.windows.size() > beforeMove);
        for (size_t i = beforeMove; i < display.windows.size(); ++i)
            assert(display.windows[i].height < 160);
    }
    matchesFullRedraw();
    game.OnRightKey(); // Exit entry scrolls into view in the two-row fixture.
    matchesFullRedraw();
    game.OnLeftKey();
    matchesFullRedraw();
    game.OnLeftKey();
    assert(display.screen == browsing);
    assert(!game.preventsIdleSleep());
    game.stopPlaybackForSleep();
    game.redrawAllNow();
    assert(display.screen == browsing);
    const size_t beforeConfirm = display.windows.size();
    game.OnConfirmKey(); // Confirmation defaults to cancel
    if (!configured)
        for (size_t i = beforeConfirm; i < display.windows.size(); ++i)
            assert(display.windows[i].y >= 88);
    assert(display.screen != browsing);
    matchesFullRedraw();
    game.OnConfirmKey();
    assert(display.screen == browsing);
    assert(pet.customStat(0) == 50 && pet.persistentState().shopQuantities[0] == 0);
    game.OnConfirmKey();
    const size_t beforeToggle = display.windows.size();
    const auto returning = display.screen;
    game.OnLeftKey();
    if (configured)
    {
        assert(display.screen != returning);
        matchesFullRedraw();
    }
    else
    {
        assert(display.windows.size() == beforeToggle + 1);
        assert(display.windows.back().y == 144 && display.windows.back().height == 16);
    }
    game.OnConfirmKey();
    assert(pet.customStat(0) == 43 && pet.persistentState().shopQuantities[0] == 2);
    matchesFullRedraw();
    game.OnConfirmKey(); // Receipt to shop
    game.OnRightKey();
    game.OnConfirmKey(); // Inventory
    matchesFullRedraw();
    game.OnConfirmKey(); // Use confirmation
    game.OnLeftKey();
    game.OnConfirmKey();
    assert(pet.customStat(0) == 46 && pet.persistentState().shopQuantities[0] == 1);
    assert(pet.stageDays() == 1 && pet.speciesSlot() == initialSpecies);
    const auto windowsBeforePlayback = display.windows.size();
    tick(game, 1);
    assert(display.windows.size() > windowsBeforePlayback);
    for (unsigned time = 250; time <= 1000; time += 250) tick(game, time);
    game.OnConfirmKey(); // Receipt to inventory
    game.OnLeftKey(); // Wrap to exit
    game.OnConfirmKey();

    Pet restored;
    PetStorage restoredStorage(&sd);
    AssetResources restoredResources(&sd);
    Renderer restoredRenderer(&display, restoredResources);
    SdAppearanceLoader restoredLoader(&sd);
    Game reboot(restored, restoredStorage, restoredRenderer, restoredResources, restoredLoader);
    assert(reboot.setup_game());
    assert(restored.customStat(0) == 46 && restored.persistentState().shopQuantities[0] == 1);
    assert(restored.stageDays() == 1 && restored.speciesSlot() == initialSpecies);
    reboot.OnRightKey();
    reboot.OnConfirmKey(); // Direct inventory command
    reboot.OnConfirmKey();
    reboot.OnLeftKey();
    sd.limitWrite(12, 0, 0);
    reboot.OnConfirmKey();
    assert(restored.customStat(0) == 46 && restored.persistentState().shopQuantities[0] == 1);
    assert(restored.stageDays() == 1 && restored.speciesSlot() == initialSpecies);

    Pet recovered;
    PetStorage recoveredStorage(&sd);
    assert(recoveredStorage.load(recovered, restored.persistentState().schemaFingerprint));
    assert(recovered.customStat(0) == 46 && recovered.persistentState().shopQuantities[0] == 1);
    assert(recovered.stageDays() == 1 && recovered.speciesSlot() == initialSpecies);

    sd.limitWrite(static_cast<size_t>(-1), 0, 0);
    reboot.OnConfirmKey(); // Dismiss failure receipt and retry.
    reboot.OnConfirmKey();
    reboot.OnLeftKey();
    reboot.OnConfirmKey();
    assert(restored.stageDays() == 2 && restored.speciesSlot() == initialSpecies);
    assert(restored.persistentState().shopQuantities[0] == 0);
    for (unsigned time = 1001; time <= 2001; time += 250) tick(reboot, time);
    assert(restored.stageDays() == 0 && restored.speciesSlot() != initialSpecies);
    assert(restored.customStat(0) == 49);
    assert(recoveredStorage.load(recovered, restored.persistentState().schemaFingerprint));
    assert(recovered.stageDays() == 0 && recovered.speciesSlot() == restored.speciesSlot());
    puts("[PASS] Exported shop: three keys, cancel, purchase, eating, stage-days evolution, reboot and interrupted commit");
    return 0;
}
#endif

#if ENABLE_STAT_EVENTS
static void verifyBingbingEvents(const std::filesystem::path &root)
{
    // The exporter sorts stable IDs lexicographically, including custom10 and custom11.
    constexpr uint8_t quota = 1, settledDays = 2, trainingFlag = 3, serviceDay = 4;
    constexpr uint8_t endingStat = 6, intelligence = 9, rank = 10;
    for (bool train : {false, true})
    {
        hostMillis = 0;
        SdFat sd;
        mountBundle(sd, root);
        Adafruit_ST7735 display;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(&display, resources);
        SdAppearanceLoader loader(&sd);
        Game game(pet, storage, renderer, resources, loader);
        assert(game.setup_game());
        PetBehaviorConfig config = {};
        assert(loadRuntimeContract(&sd, pet.speciesSlot(), pet.outfitSlot(), config));
        CommandController buttons;
        buttons.configure(config);
        buttons.resetSelection();
        const auto select = [&](int slot)
        {
            assert(buttons.isSlotVisible(slot));
            while (buttons.selectedSlot() != slot)
            {
                buttons.next();
                game.OnRightKey();
            }
        };
        assert(pet.customStat(serviceDay) == 1 && pet.customStat(settledDays) == 0);
        select(5);
        for (unsigned int day = 1; day <= 13; ++day)
        {
            const auto sequence = storage.lastSaveSequence();
            game.OnConfirmKey();
            assert(!game.isRoundComplete() && pet.customStat(settledDays) == day);
            assert(pet.customStat(rank) == (day == 1 ? 1 : 2));
            assert(storage.lastSaveSequence() == sequence + 1);
        }
        tick(game, 100000);
        assert(pet.customStat(serviceDay) == 14 && pet.customStat(settledDays) == 13 && pet.customStat(endingStat) == 0);
        assert(pet.isCollectionOwned(16) && pet.isCollectionOwned(17));
        if (train)
        {
            select(1);
            game.OnConfirmKey();
            assert(pet.customStat(intelligence) == 110 && pet.customStat(quota) == 9 && pet.customStat(trainingFlag) == 1);
            assert(!game.hasTransientAnimation());
            select(5);
        }
        const auto before = pet.persistentState();
        const auto sequence = storage.lastSaveSequence();
        sd.limitWrite(12, 0, 0);
        game.OnConfirmKey();
        assert(!game.isRoundComplete() && memcmp(&before, &pet.persistentState(), sizeof(before)) == 0);
        sd.limitWrite(static_cast<size_t>(-1), 0, 0);
        game.OnConfirmKey();
        const uint8_t ending = train ? 5 : 2;
        const uint8_t endingCollectionSlot = 31 + ending - 1;
        assert(game.isRoundComplete() && pet.customStat(settledDays) == 14 && pet.customStat(quota) == 10);
        assert(pet.customStat(endingStat) == ending && pet.isCollectionOwned(endingCollectionSlot));
        assert(storage.lastSaveSequence() == sequence + 1);
        tick(game, 100001);
        assert(display.screen[0] == (train ? 0x001f : 0x07e0));
        tick(game, 100700);
        assert(display.screen[0] == (train ? 0x07ff : 0xffe0));

        Pet reopened;
        PetStorage reopenStorage(&sd);
        AssetResources reopenResources(&sd);
        Renderer reopenRenderer(&display, reopenResources);
        SdAppearanceLoader reopenLoader(&sd);
        Game reopenedGame(reopened, reopenStorage, reopenRenderer, reopenResources, reopenLoader);
        assert(reopenedGame.setup_game() && reopenedGame.isRoundComplete());
        assert(reopened.customStat(endingStat) == ending && reopened.isCollectionOwned(endingCollectionSlot));
        reopenedGame.OnRightKey();
        assert(reopenedGame.isRoundComplete());
        reopenedGame.OnConfirmKey();
        assert(!reopenedGame.isRoundComplete() && reopened.customStat(serviceDay) == 1 && reopened.customStat(settledDays) == 0);
        assert(reopened.customStat(endingStat) == 0 && reopened.isCollectionOwned(endingCollectionSlot));
    }
    {
        hostMillis = 0;
        SdFat sd;
        mountBundle(sd, root);
        Adafruit_ST7735 display;
        Pet pet;
        PetStorage storage(&sd);
        AssetResources resources(&sd);
        Renderer renderer(&display, resources);
        SdAppearanceLoader loader(&sd);
        Game game(pet, storage, renderer, resources, loader);
        assert(game.setup_game());
        const auto recruit = pet.speciesSlot();
        pet.setCustomStat(7, 150);
        pet.setCustomStat(8, 150);
        pet.setCustomStat(9, 150);
        assert(game.saveNow());
        PetBehaviorConfig config = {};
        assert(loadRuntimeContract(&sd, pet.speciesSlot(), pet.outfitSlot(), config));
        CommandController buttons;
        buttons.configure(config);
        buttons.resetSelection();
        while (buttons.selectedSlot() != 5)
        {
            buttons.next();
            game.OnRightKey();
        }
        game.OnConfirmKey();
        game.OnConfirmKey();
        const auto before = pet.persistentState();
        const auto sequence = storage.lastSaveSequence();
        sd.limitWrite(12, 0, 0);
        game.OnConfirmKey();
        assert(memcmp(&before, &pet.persistentState(), sizeof(before)) == 0);
        sd.limitWrite(static_cast<size_t>(-1), 0, 0);
        game.OnConfirmKey();
        assert(pet.speciesSlot() != recruit && pet.stageDays() == 0 && pet.customStat(settledDays) == 3);
        assert(pet.customStat(11) == 1 && pet.isCollectionOwned(0));
        assert(storage.lastSaveSequence() == sequence + 1);
        Pet reopened;
        PetStorage reopenStorage(&sd);
        assert(reopenStorage.load(reopened, pet.persistentState().schemaFingerprint));
        assert(reopened.speciesSlot() == pet.speciesSlot() && reopened.customStat(11) == 1 && reopened.isCollectionOwned(0));
    }
    puts("[PASS] Bingbing SD: day 14 remains playable until bedtime; branches and fallback, animation-free training, full-screen frames, atomic evolution and retry, reboot, and collection retention.");
}
#endif

int main(int argc, char **argv)
{
#if ENABLE_CAPTURE_GAME
    if (argc == 3 && (strcmp(argv[2], "capture") == 0 || strcmp(argv[2], "capture-live-clock") == 0))
    {
        verifyPreCaptureSave();
        verifyCapture(argv[1], strcmp(argv[2], "capture") == 0);
        return 0;
    }
#endif
    assert(argc == 2 || argc == 3);
#if ENABLE_STAT_EVENTS
    if (argc == 3 && std::string(argv[2]) == "stat-events") { verifyBingbingEvents(argv[1]); return 0; }
#endif
    if (argc == 3 && std::string(argv[2]) == "transactions") { verifyTransactions(); return 0; }
#if ENABLE_SHOP
    if (argc == 3 && std::string(argv[2]) == "shop") return verifyShop(argv[1]);
#endif
    if (argc == 2 || std::string(argv[2]) == "single") verifyCatchUpCollectionWindow(argv[1]);
#if COLLECTION_USE_RTC
    if (argc == 3 && std::string(argv[2]) == "guess-classic")
    {
        verifyClassicGuess(argv[1]);
        return 0;
    }
#endif
    if (argc == 3 && std::string(argv[2]) == "capacity")
    {
        unsigned rejected = 0;
        // Product validation belongs to the exporter. Firmware retains only
        // the limits required by its 64-bit ownership mask and eight counters.
        for (const char *name : {"quantity", "action-capacity"})
        {
            const auto fixture = std::filesystem::path(argv[1]) / name;
            SdFat damaged; mountBundle(damaged, fixture);
            Adafruit_ST7735 display; Pet pet; PetStorage storage(&damaged);
            AssetResources resources(&damaged);
            Renderer renderer(&display, resources); SdAppearanceLoader loader(&damaged);
            Game game(pet, storage, renderer, resources, loader);
            assert(!game.setup_game());
            assert(!damaged.exists("/state_a.bin") && !damaged.exists("/state_b.bin"));
            printf("[PASS] Production startup rejects compiled capacity overflow: %s\n", name);
            ++rejected;
        }
        assert(rejected == 2);
        return 0;
    }
    SdFat sd;
    const bool recoveryMode = argc == 3 && std::string(argv[2]) == "recovery";
    mountBundle(sd, recoveryMode ? std::filesystem::path(argv[1]) / "base" : std::filesystem::path(argv[1]));
    Adafruit_ST7735 display;
    Pet pet;
    PetStorage storage(&sd);
    AssetResources resources(&sd);
    Renderer renderer(&display, resources);
    SdAppearanceLoader loader(&sd);
    Game game(pet, storage, renderer, resources, loader);
    assert(game.setup_game());
#if ENABLE_PROJECT_MENUS
    if (argc == 3 && std::string(argv[2]) == "project-menus")
    {
        PetBehaviorConfig config = {};
        assert(loadRuntimeContract(&sd, pet.speciesSlot(), pet.outfitSlot(), config));
        CommandController expected;
        expected.configure(config);
        expected.resetSelection();
        const auto selectSlot = [&](int slot)
        {
            assert(expected.isSlotVisible(slot));
            while (expected.selectedSlot() != slot)
            {
                expected.next();
                game.OnRightKey();
            }
        };
        const auto rows = [&display](int count)
        {
            return std::vector<uint16_t>(display.screen.begin(), display.screen.begin() + 128 * 16 * count);
        };

        const PersistedPetState before = pet.persistentState();
        selectSlot(3);
        game.OnConfirmKey();
        assert(game.isProjectMenuActive());
        const auto training = rows(5);
        game.OnConfirmKey(); // A named training row with no game binding.
        assert(game.isProjectMenuActive() && rows(5) == training);
        game.OnLeftKey();
        game.OnConfirmKey(); // Generated Back wraps from first to last.
        assert(!game.isProjectMenuActive());

        selectSlot(4);
        game.OnConfirmKey();
        assert(game.isProjectMenuActive());
        const auto directory = rows(5);
        game.OnConfirmKey(); // First text card.
        assert(game.isProjectMenuActive() && rows(5) != directory);
        game.OnConfirmKey();
        assert(rows(5) == directory);

        game.OnRightKey();
        const auto collectionSelected = rows(5);
        game.OnConfirmKey();
        assert(game.isCollectionCatalogActive());
        game.OnConfirmKey(); // Collection starts on its own Back tile.
        assert(game.isProjectMenuActive() && rows(5) == collectionSelected);

        game.OnRightKey();
        game.OnRightKey();
        const auto nestedSelected = rows(5);
        game.OnConfirmKey();
        assert(game.isProjectMenuActive() && rows(2) !=
               std::vector<uint16_t>(nestedSelected.begin(), nestedSelected.begin() + 128 * 32));
        game.OnProjectMenuBack();
        assert(game.isProjectMenuActive() && rows(5) == nestedSelected);
        game.OnProjectMenuBack();
        assert(!game.isProjectMenuActive());
        assert(memcmp(&before, &pet.persistentState(), sizeof(before)) == 0);
        puts("[PASS] Exported project menus: root bindings, blank training action, card, collection, nested selection, and both exits preserve Pet State");
        return 0;
    }
#endif
    if (argc == 3 && std::string(argv[2]) == "manual-day")
    {
        assert(pet.customStat(2) == 10 && pet.stageDays() == 0);
        for (int i = 0; i < 10; ++i) game.OnConfirmKey();
        assert(pet.customStat(2) == 0);
        const auto exhaustedSequence = storage.lastSaveSequence();
        game.OnConfirmKey();
        assert(pet.customStat(2) == 0 && storage.lastSaveSequence() == exhaustedSequence);
        tick(game, 100000);
        assert(pet.stageDays() == 0);
        for (int i = 0; i < 4; ++i) game.OnRightKey();
        sd.limitWrite(12, 0, 0);
        game.OnConfirmKey();
        assert(pet.stageDays() == 0 && pet.customStat(2) == 0);
        Pet old; PetStorage oldStorage(&sd);
        assert(oldStorage.load(old, pet.persistentState().schemaFingerprint));
        assert(old.stageDays() == 0 && old.customStat(2) == 0);
        sd.limitWrite(static_cast<size_t>(-1), 0, 0);
        game.OnConfirmKey();
        assert(pet.stageDays() == 1 && pet.customStat(2) == 10);
        Pet restored; PetStorage rebootStorage(&sd);
        assert(rebootStorage.load(restored, pet.persistentState().schemaFingerprint));
        assert(restored.stageDays() == 1 && restored.customStat(2) == 10);
        tick(game, 200000);
        assert(pet.stageDays() == 1);
        puts("[PASS] Exported manual-day bundle: ten actions, quota rejection, RTC suppression, torn day write rollback, retry and complete A/B recovery");
        return 0;
    }
    if (argc == 3 && std::string(argv[2]) == "guess")
    {
        assert(!pet.isCollectionOwned(0) && !pet.isCollectionOwned(1));
        assert(!pet.isOutfitUnlocked(3) && !pet.isOutfitUnlocked(4));
        game.OnRightKey();
        FirmwareRandom::seed(1); // First item-side sample is Right.
        game.OnConfirmKey();
        tick(game, 51); // The real MinigameController enters WaitingInput.
        const uint32_t before = storage.lastSaveSequence();
        game.OnRightKey(); // Round=10, then final=20 in the same dispatch.
        assert(pet.customStat(0) == 20);
        assert(pet.isCollectionOwned(0) && pet.isCollectionOwned(1));
        assert(pet.isOutfitUnlocked(3) && pet.isOutfitUnlocked(4));
        assert(storage.lastSaveSequence() == before + 2);
        assert(game.hasTransientAnimation()); // Both snapshots precede playback completion.
        Pet restored; PetStorage rebootStorage(&sd);
        assert(rebootStorage.load(restored, pet.persistentState().schemaFingerprint));
        assert(restored.customStat(0) == 20 && restored.isCollectionOwned(0) && restored.isCollectionOwned(1));
        assert(restored.isOutfitUnlocked(3) && restored.isOutfitUnlocked(4));
        // Reset and fail the final snapshot after a successful round snapshot.
        assert(game.resetPet());
        FirmwareRandom::seed(1);
        game.OnRightKey(); // Reset selects the first button (catalog); select Guess again.
        game.OnConfirmKey();
        tick(game, 102);
        sd.limitWriteAfter(1, 12);
        game.OnRightKey();
        Pet recovered; PetStorage recovery(&sd);
        assert(recovery.load(recovered, pet.persistentState().schemaFingerprint));
        assert(recovered.customStat(0) == 10 && recovered.isCollectionOwned(0));
        assert(!recovered.isCollectionOwned(1) && recovered.isOutfitUnlocked(3) && !recovered.isOutfitUnlocked(4));
        puts("[PASS] Public Guess export: real round and final settlements save transient acquisition and outfit grants before animation, reboot restores final state, interrupted final save restores complete round snapshot");
        return 0;
    }
    if (recoveryMode)
    {
        verifyTransactions();
        CollectionHost::mounted = &sd;
        pet.setCustomStat(0, 17); pet.setStageDays(9);
        pet.acquireCollection(0); pet.incrementActionCount(0);
        assert(game.saveNow());
        const auto boot = [&](const char *variant, bool owned, uint16_t count, int16_t stat, uint32_t days) {
            mountBundle(sd, std::filesystem::path(argv[1]) / variant);
            Pet restored; PetStorage rebootStorage(&sd);
            AssetResources rebootRendererResources(&sd);
            Renderer rebootRenderer(&display, rebootRendererResources); SdAppearanceLoader rebootLoader(&sd);
            Game reboot(restored, rebootStorage, rebootRenderer, rebootRendererResources, rebootLoader);
            assert(reboot.setup_game());
            assert(restored.isCollectionOwned(0) == owned && restored.actionCount(0) == count);
            assert(restored.customStat(0) == stat && restored.stageDays() == days);
        };
        boot("base", true, 1, 17, 9);
        boot("content-only", true, 1, 17, 9);
        boot("quantity", false, 1, 17, 9);
        boot("reorder", false, 1, 17, 9);
        Pet owned; PetStorage ownershipCommit(&sd);
        assert(ownershipCommit.load(owned, pet.persistentState().schemaFingerprint));
        owned.acquireCollection(0);
        assert(ownershipCommit.save(owned));
        boot("action-map", true, 0, 17, 9);
        boot("disabled", false, 0, 17, 9);
        boot("schema", false, 0, 0, 0);
        puts("[PASS] Public replacement bundles: reboot and content edits preserve progress; ordered identities clear ownership independently; action mapping clears counts; schema resets Pet without migration");
        return 0;
    }
    if (argc == 3 && std::string(argv[2]) == "counts")
    {
        // Real button dispatch commits effects + counts + acquisition together.
        game.OnRightKey();
        game.OnConfirmKey();
        assert(pet.actionCount(0) == 1 && pet.customStat(0) == 1 && !pet.isCollectionOwned(0));
        for (unsigned long now = 50; now <= 1000; now += 50) tick(game, now);
        assert(pet.actionCount(0) == 1); // five repeats do not count again
        game.OnConfirmKey();
        assert(pet.actionCount(0) == 2 && !pet.isCollectionOwned(0));
        for (unsigned long now = 1050; now <= 2000; now += 50) tick(game, now);
        // At the RTC boundary, the animation may finish after synchronizeTime
        // attempts the day. Drain that pending complete day before dispatching
        // the next command; production input intentionally waits for catch-up.
        tick(game, 2050);
        game.OnRightKey();
        game.OnConfirmKey();
        assert(pet.actionCount(1) == 1 && pet.isCollectionOwned(0));
        Pet committed;
        PetStorage restoredStorage(&sd);
        assert(restoredStorage.load(committed, pet.persistentState().schemaFingerprint));
        assert(committed.actionCount(0) == 2 && committed.actionCount(1) == 1 && committed.isCollectionOwned(0));
        assert(committed.customStat(0) == pet.customStat(0));
        pet.setSpeciesSlot(pet.speciesSlot());
        pet.setOutfitSlot(pet.outfitSlot());
        assert(pet.actionCount(0) == 2);
        for (unsigned index = 2; index < 65540; ++index) pet.incrementActionCount(0);
        assert(pet.actionCount(0) == 65535);
        assert(game.saveNow());
        const uint32_t sequence = storage.lastSaveSequence();
        pet.setCustomStat(0, 42);
        pet.incrementActionCount(1);
        sd.limitWrite(12, 0, 0);
        assert(!game.saveNow() && storage.lastSaveSequence() == sequence + 1);
        Pet recovered;
        PetStorage recovery(&sd);
        assert(recovery.load(recovered, pet.persistentState().schemaFingerprint));
        assert(recovered.persistentState().sequence == sequence);
        assert(recovered.actionCount(0) == 65535 && recovered.actionCount(1) == 1 && recovered.customStat(0) != 42);
        recovered.configureCollection(recovered.persistentState().collectionFingerprint + 1);
        assert(!recovered.isCollectionOwned(0) && recovered.actionCount(0) == 65535);
        recovered.configureTrackedActions(recovered.persistentState().trackedActionFingerprint + 1);
        assert(recovered.actionCount(0) == 0);
        pet.setOutfitSlot(2);
        assert(pet.actionCount(0) == 0 && pet.isCollectionOwned(0));
        pet.incrementActionCount(0);
        pet.setSpeciesSlot(2);
        assert(pet.actionCount(0) == 0);
        // All eight compiled mappings use the production action dispatcher,
        // including a valid action whose optional playback is absent.
        PetBehaviorConfig config = {}; char resource[20] = {};
        assert(loadRuntimeContract(&sd, 1, 1, config, resource, sizeof(resource)));
        AnimationController animations(renderer, resources);
        animations.configureRuntimeContract(config);
        Pet isolated;
        PetBehaviorRuntime runtime(config, isolated, animations, resources);
        runtime.initializeStats();
        for (uint8_t slot = 0; slot < 8; ++slot)
        {
            assert(runtime.executeAction(config.trackedActions[slot]) != PetBehaviorActionResult::Rejected);
            assert(isolated.actionCount(slot) == 1);
        }
        const auto beforeUnknown = isolated.persistentState();
        assert(runtime.executeAction(255) == PetBehaviorActionResult::Rejected);
        assert(memcmp(&beforeUnknown, &isolated.persistentState(), sizeof(beforeUnknown)) == 0);
        config.actionOutcomes[config.actions[0].firstOutcome].animationPlayback = {};
        assert(runtime.executeAction(0) == PetBehaviorActionResult::Applied);
        assert(isolated.actionCount(0) == 2); // absent optional playback still dispatches once
        config.trackedActionCount = 0;
        assert(runtime.executeAction(0) != PetBehaviorActionResult::Rejected);
        assert(isolated.actionCount(0) == 2); // valid untracked dispatch
        pet.setDefaultState();
        assert(pet.actionCount(1) == 0 && !pet.isCollectionOwned(0));
        puts("[PASS] Public six-AND/eight-action export: production dispatch/replays, saturation, atomic storage, identity resets and independent fingerprints");
        return 0;
    }
    if (argc == 3 && std::string(argv[2]) == "activation-evolution")
    {
        pet.setCustomStat(0, 1); assert(game.saveNow());
        game.OnConfirmKey(); game.OnRightKey(); game.OnConfirmKey();
        assert(pet.speciesSlot() == 2 && pet.stageDays() == 0);
        pet.incrementActionCount(0);
        tick(game, 1000);
        assert(pet.speciesSlot() == 2); // no synchronous/early evolution
        tick(game, 2000);
        // Daily stage advance makes the real relationship eligible, and normal
        // Evolution entry resets counts instead of resurrecting old counters.
        assert(pet.speciesSlot() == 3 && pet.outfitSlot() == 1 && pet.stageDays() == 0 && pet.actionCount(0) == 0);
        assert(pet.isCollectionOwned(0));
        assert(game.resetPet());
        assert(pet.speciesSlot() == 1 && pet.outfitSlot() == 1 && pet.stageDays() == 0);
        assert(pet.customStat(0) == 0 && pet.actionCount(0) == 0 && !pet.isCollectionOwned(0));
        Pet restored; PetStorage rebootStorage(&sd);
        assert(rebootStorage.load(restored, pet.persistentState().schemaFingerprint));
        assert(restored.customStat(0) == 0 && !restored.isCollectionOwned(0) && restored.actionCount(0) == 0);
        puts("[PASS] Public Evolution bundle: collection target waits for normal Pet day; real Evolution resets identity counts; production whole reset persists fresh state");
        return 0;
    }
    if (argc == 3 && std::string(argv[2]) == "activation")
    {
        const auto activate = [&](uint8_t slot) {
            game.OnConfirmKey();
            assert(game.isCollectionCatalogActive());
            for (uint8_t step = 0; step <= slot; ++step) game.OnRightKey();
            game.OnConfirmKey();
        };
        pet.setCustomStat(0, 1);
        assert(game.saveNow());
        assert(pet.isCollectionOwned(0) && pet.isCollectionOwned(1) && pet.isCollectionOwned(2));
        assert(!pet.isCollectionOwned(3));
        activate(3);
        assert(game.isCollectionCatalogActive() && pet.speciesSlot() == 1); // hint only
        game.OnConfirmKey();
        for (uint8_t step = 0; step < 4; ++step) game.OnLeftKey();
        game.OnConfirmKey();
        pet.setStageDays(7);
        pet.incrementActionCount(0);
        const uint32_t beforeSpecies = storage.lastSaveSequence();
        activate(0);
        assert(!game.isCollectionCatalogActive() && pet.speciesSlot() == 2 && pet.outfitSlot() == 1);
        assert(pet.stageDays() == 0 && pet.customStat(0) == 1 && pet.actionCount(0) == 0);
        assert(storage.lastSaveSequence() == beforeSpecies + 1);
        assert(pet.isOutfitUnlocked(2)); // owned collection overrides unmet original predicate
        pet.setStageDays(9);
        pet.incrementActionCount(0);
        activate(1);
        assert(pet.speciesSlot() == 2 && pet.outfitSlot() == 2 && pet.stageDays() == 9);
        assert(pet.actionCount(0) == 0 && pet.customStat(0) == 1);
        pet.incrementActionCount(0);
        activate(1);
        assert(pet.actionCount(0) == 1 && pet.stageDays() == 9); // same target never resets
        activate(2);
        assert(pet.speciesSlot() == 1 && pet.stageDays() == 0 && pet.actionCount(0) == 0);
        activate(0);
        assert(pet.speciesSlot() == 2 && pet.isOutfitUnlocked(2)); // grant derived on revisit
        activate(1);
        pet.setStageDays(5); pet.incrementActionCount(0);
        game.OnRightKey(); game.OnRightKey(); game.OnConfirmKey();
        game.OnLeftKey(); game.OnConfirmKey(); // ordinary Outfit2 -> Outfit1
        assert(pet.speciesSlot() == 2 && pet.outfitSlot() == 1);
        assert(pet.stageDays() == 5 && pet.actionCount(0) == 0 && pet.isCollectionOwned(1));
        pet.incrementActionCount(0);
        game.OnConfirmKey(); game.OnConfirmKey(); // same ordinary target
        assert(pet.actionCount(0) == 1 && pet.stageDays() == 5);
        game.OnLeftKey(); game.OnLeftKey(); activate(1);
        assert(pet.outfitSlot() == 2 && pet.actionCount(0) == 0);
        Pet restored;
        PetStorage rebootStorage(&sd);
        AssetResources rebootRendererResources(&sd);
        Renderer rebootRenderer(&display, rebootRendererResources);
        SdAppearanceLoader rebootLoader(&sd);
        Game reboot(restored, rebootStorage, rebootRenderer, rebootRendererResources, rebootLoader);
        assert(reboot.setup_game());
        assert(restored.speciesSlot() == 2 && restored.outfitSlot() == 2 && restored.isOutfitUnlocked(2));
        assert(restored.customStat(0) == 1 && restored.isCollectionOwned(0));
        const uint32_t previousSequence = restored.persistentState().sequence;
        sd.limitWrite(12, 0, 0);
        reboot.OnConfirmKey();
        for (uint8_t step = 0; step < 3; ++step) reboot.OnRightKey();
        reboot.OnConfirmKey();
        assert(rebootStorage.lastSaveSequence() == previousSequence + 1);
        Pet recovered;
        PetStorage recovery(&sd);
        assert(recovery.load(recovered, restored.persistentState().schemaFingerprint));
        assert(recovered.persistentState().sequence == previousSequence);
        assert(recovered.speciesSlot() == 2 && recovered.outfitSlot() == 2 && recovered.isCollectionOwned(1));
        puts("[PASS] Public activation export: unowned hint, species/outfit targets, identity semantics, reusable grants, single atomic save, reboot and interrupted switching");
        return 0;
    }
    assert(!pet.isCollectionOwned(0));
    if (argc == 3 && (strcmp(argv[2], "grid64") == 0 || strcmp(argv[2], "pages64") == 0))
    {
        verify64Grid(game, pet, display, strcmp(argv[2], "pages64") == 0);
        return 0;
    }
    tick(game, 1000);
    game.OnConfirmKey();
    assert(game.isCollectionCatalogActive());
    const auto grid = display.screen;
    const size_t before = display.windows.size();
    game.OnRightKey();
    // The decoder uses one write window per rectangle: previous/new focus only.
    assert(display.windows.size() == before + 2);
    game.OnConfirmKey();
    const auto hint = display.screen;
    assert(hint != grid);
    game.OnLeftKey();
    assert(display.screen == hint);
    tick(game, 101000);
    assert(pet.stageDays() == 0);
    game.redrawAllNow();
    assert(display.screen == hint);
    game.OnConfirmKey();
    game.OnLeftKey();
    game.OnConfirmKey();
    assert(!game.isCollectionCatalogActive());
    tick(game, 101999);
    assert(pet.stageDays() == 0);
    tick(game, 102000);
    assert(pet.stageDays() == 1);
    tick(game, 104000);
    assert(pet.stageDays() == 2);
    assert(pet.isCollectionOwned(0));
    game.OnConfirmKey();
    game.OnRightKey();
    game.OnConfirmKey();
    assert(display.screen != hint);
    // Reboot uses the production A/B persistence snapshot, no catalog sidecar.
    Pet restored;
    PetStorage rebootStorage(&sd);
    AssetResources rebootRendererResources(&sd);
    Renderer rebootRenderer(&display, rebootRendererResources);
    SdAppearanceLoader rebootLoader(&sd);
    Game reboot(restored, rebootStorage, rebootRenderer, rebootRendererResources, rebootLoader);
    assert(reboot.setup_game());
    assert(restored.stageDays() == 2 && restored.isCollectionOwned(0));
    // Failed next-slot write keeps the previous complete Pet+catalog transaction.
    restored.setCustomStat(0, 42);
    restored.acquireCollection(1);
    sd.limitWrite(12, 0, 0);
    assert(!reboot.saveNow());
    Pet recovered;
    PetStorage recoveryStorage(&sd);
    assert(recoveryStorage.load(recovered, pet.persistentState().schemaFingerprint));
    assert(recovered.customStat(0) == 2 && recovered.isCollectionOwned(0));
    assert(!recovered.isCollectionOwned(1));
    assert(recovered.configureCollection(pet.persistentState().collectionFingerprint + 1));
    assert(!recovered.isCollectionOwned(0) && recovered.customStat(0) == 2);
    puts("[PASS] Real collection export: production Game command, pixels, pause remainder, acquisition, reboot and interrupted A/B transaction");
}
