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
