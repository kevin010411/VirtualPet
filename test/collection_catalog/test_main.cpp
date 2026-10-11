#include <assert.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <stdio.h>
#include <string.h>
#include "controller/Game.h"
#include "appearance/SdAppearanceLoader.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetStorage.h"
#include "pet/PetBehaviorRuntime.h"
#include "animation/AnimationController.h"
#include "resources/RuntimeContractLoader.h"
#include "common/FirmwareRandom.h"

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

#if COLLECTION_USE_RTC
static void verifyClassicGuess(const std::filesystem::path &root)
{
    for (bool interrupted : {false, true})
    {
        hostMillis = 0;
        SdFat sd; mountBundle(sd, root);
        Adafruit_ST7735 display; Pet pet; PetStorage storage(&sd);
        Renderer renderer(&display, &sd); SdAppearanceLoader loader(&sd);
        Game game(pet, storage, renderer, loader);
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
    puts("[PASS] Production v16 A/B writer: all 96 truncation boundaries, complete failed sync, newest CRC fallback and both-corrupt discard recover whole snapshots");
}

static void verify64Grid(Game &game, Pet &pet, Adafruit_ST7735 &display)
{
    const auto pixel = [&](int x, int y) { return display.screen[y * 128 + x]; };
    game.OnConfirmKey();
    assert(game.isCollectionCatalogActive());
    assert(pixel(0,0) == 0x07ff); // Author-supplied selected return image.
    const size_t before = display.windows.size();
    game.OnRightKey();
    assert(display.windows.size() == before + 2);
    assert(pixel(0,0) == 0xffe0 && pixel(32,0) == 0xffff);
    assert(pixel(48,16) == 0x001f); // Shared unknown image, opaque blue center.
    assert(pixel(36,4) == 0x0820); // Transparency composed against this cell's background.
    game.OnLeftKey();
    assert(pixel(32,0) == 0x0820); // Removing selection restores the original background.
    game.OnLeftKey(); // First return wraps to item64, fifth page, cell4.
    assert(display.windows.back().x == 0 && display.windows.back().y == 32);
    assert(pixel(0,32) == 0xffff && pixel(16,48) == 0x001f);
    assert(pixel(32,32) == 0x28a0); // Empty cell5 stays background, never an item.
    const auto fifthFocused = display.screen;
    game.OnConfirmKey();
    assert(pixel(0,0) == 0xf800 && pixel(127,159) == 0xf800);
    game.OnConfirmKey();
    assert(display.screen == fifthFocused); // Detail close restores exact page/focus.
    const size_t beforeWrap = display.windows.size();
    game.OnRightKey();
    assert(display.windows.size() == beforeWrap + 17); // Full background +16 cells.
    assert(pixel(0,0) == 0x07ff); // Last item skips all empty cells and wraps.
    // Each page has its own return cell and every return exits the production caller.
    for (int page = 0; page < 5; ++page)
    {
        if (page != 0) game.OnConfirmKey();
        for (int step = 0; step < page * 16; ++step) game.OnRightKey();
        assert(pixel(0,0) == 0x07ff);
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
    assert(pixel(0,0) == 0x07e0 && pixel(127,159) == 0x07e0); // Story descriptor63.
    game.OnConfirmKey();
    assert(pixel(0,32) == 0xffff && pixel(16,48) == 0xf800);
    puts("[PASS] Real64 bundle: five pages, cyclic navigation, blank skipping, all returns, transparent per-cell composition, final tile/story frames, exact detail focus and two-cell redraw");
}

int main(int argc, char **argv)
{
    assert(argc == 2 || argc == 3);
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
            Renderer renderer(&display, &damaged); SdAppearanceLoader loader(&damaged);
            Game game(pet, storage, renderer, loader);
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
    Renderer renderer(&display, &sd);
    SdAppearanceLoader loader(&sd);
    Game game(pet, storage, renderer, loader);
    assert(game.setup_game());
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
        game.OnConfirmKey(); // reset preserves the Guess command selection
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
            Renderer rebootRenderer(&display, &sd); SdAppearanceLoader rebootLoader(&sd);
            Game reboot(restored, rebootStorage, rebootRenderer, rebootLoader);
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
        AnimationController animations(renderer);
        animations.configureRuntimeContract(config);
        Pet isolated;
        PetBehaviorRuntime runtime(config, isolated, animations, renderer);
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
        Renderer rebootRenderer(&display, &sd);
        SdAppearanceLoader rebootLoader(&sd);
        Game reboot(restored, rebootStorage, rebootRenderer, rebootLoader);
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
    if (argc == 3 && strcmp(argv[2], "grid64") == 0)
    {
        verify64Grid(game, pet, display);
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
    Renderer rebootRenderer(&display, &sd);
    SdAppearanceLoader rebootLoader(&sd);
    Game reboot(restored, rebootStorage, rebootRenderer, rebootLoader);
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
