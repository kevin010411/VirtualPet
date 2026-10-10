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

unsigned long hostMillis = 0;

static void tick(Game &game, unsigned long now)
{
    hostMillis = now;
#if COLLECTION_USE_RTC
    game.synchronizeTime(static_cast<uint32_t>(now / 1000));
#endif
    game.loop_game();
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
    SdFat sd;
    for (const auto &entry : std::filesystem::recursive_directory_iterator(argv[1]))
    {
        if (!entry.is_regular_file()) continue;
        std::ifstream input(entry.path(), std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
        const auto path = "/" + std::filesystem::relative(entry.path(), argv[1]).generic_string();
        sd.seedReadOnly(path.c_str(), bytes);
    }
    Adafruit_ST7735 display;
    Pet pet;
    PetStorage storage(&sd);
    Renderer renderer(&display, &sd);
    SdAppearanceLoader loader(&sd);
    Game game(pet, storage, renderer, loader);
    assert(game.setup_game());
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
        assert(!game.saveNow() && storage.lastSaveSequence() == sequence);
        Pet recovered;
        PetStorage recovery(&sd);
        assert(recovery.load(recovered, pet.persistentState().schemaFingerprint));
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
        pet.setDefaultState();
        assert(pet.actionCount(1) == 0 && !pet.isCollectionOwned(0));
        puts("[PASS] Public six-AND/eight-action export: production dispatch/replays, saturation, atomic storage, identity resets and independent fingerprints");
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
        Pet restored;
        PetStorage rebootStorage(&sd);
        Renderer rebootRenderer(&display, &sd);
        SdAppearanceLoader rebootLoader(&sd);
        Game reboot(restored, rebootStorage, rebootRenderer, rebootLoader);
        assert(reboot.setup_game());
        assert(restored.speciesSlot() == 2 && restored.outfitSlot() == 2 && restored.isOutfitUnlocked(2));
        assert(restored.customStat(0) == 1 && restored.isCollectionOwned(0));
        const uint32_t previousSequence = rebootStorage.lastSaveSequence();
        sd.limitWrite(12, 0, 0);
        reboot.OnConfirmKey();
        for (uint8_t step = 0; step < 3; ++step) reboot.OnRightKey();
        reboot.OnConfirmKey();
        assert(rebootStorage.lastSaveSequence() == previousSequence);
        Pet recovered;
        PetStorage recovery(&sd);
        assert(recovery.load(recovered, restored.persistentState().schemaFingerprint));
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
