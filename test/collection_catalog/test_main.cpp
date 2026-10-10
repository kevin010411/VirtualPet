#include <assert.h>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <stdio.h>
#include "controller/Game.h"
#include "appearance/SdAppearanceLoader.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetStorage.h"

unsigned long hostMillis = 0;

int main(int argc, char **argv)
{
    assert(argc == 2);
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
    hostMillis = 750;
    game.loop_game();
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
    hostMillis = 100750;
    game.loop_game();
    assert(pet.stageDays() == 0);
    game.redrawAllNow();
    assert(display.screen == hint);
    game.OnConfirmKey();
    game.OnLeftKey();
    game.OnConfirmKey();
    assert(!game.isCollectionCatalogActive());
    hostMillis = 101999;
    game.loop_game();
    assert(pet.stageDays() == 0);
    hostMillis = 102000;
    game.loop_game();
    assert(pet.stageDays() == 1);
    hostMillis = 104000;
    game.loop_game();
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
