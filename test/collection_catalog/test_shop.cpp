#include "test_cases.h"

#include <algorithm>
#include <assert.h>
#include <fstream>
#include <stdio.h>
#include <string.h>
#include <vector>

#include "appearance/SdAppearanceLoader.h"
#include "controller/Game.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetStorage.h"
#include "resources/AssetResources.h"
#include "resources/RuntimeContractLoader.h"
#include "resources/detail/RuntimeTableReader.h"

int verifyShop(const std::filesystem::path &root)
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
    shopData.seekg(20);
    uint8_t artBytes[2];
    shopData.read(reinterpret_cast<char *>(artBytes), sizeof(artBytes));
    const uint16_t art = RuntimeTableInternal::readU16(artBytes);
    shopData.seekg(23);
    const int items = shopData.get(), products = shopData.get();
    const bool list = shopData.get() == 16;
    const int species = shopData.get();
    const auto confirmationOffset = 39 + (129 + 2 * species) * items + 70 * products +
                                    (list ? 0 : 4 * (items + products + 3));
    shopData.seekg(confirmationOffset);
    char confirmationTag[4]{};
    shopData.read(confirmationTag, sizeof(confirmationTag));
    const bool configured = std::string(confirmationTag, sizeof(confirmationTag)) == "SCF1";
    shopData.clear();
    shopData.seekg(-4, std::ios::end);
    char bandTag[4]{};
    shopData.read(bandTag, sizeof(bandTag));
    const bool styledBrowse = std::string(bandTag, sizeof(bandTag)) == "SBD2";
    const bool banded = styledBrowse || std::string(bandTag, sizeof(bandTag)) == "SBD1";
    const auto checkButtons = [&](bool inventory, bool next) {
        const size_t firstWindow = display.windows.size();
        const size_t firstFill = display.filledRects.size();
        const auto prior = display.screen;
        if (next) game.OnRightKey();
        else game.OnLeftKey();
        assert(display.screen != prior);
        if (configured)
        {
            const auto readWord = [&](int offset, unsigned bytes) {
                shopData.seekg(confirmationOffset + offset);
                unsigned value = 0;
                for (unsigned i = 0; i < bytes; ++i)
                    value |= static_cast<unsigned>(shopData.get()) << (8 * i);
                return value;
            };
            const int page = readWord(4 + 4 * (6 + (inventory ? 0 : items)), 4);
            const int count = readWord(page, 1);
            Adafruit_ST7735::Window buttons[2];
            uint16_t topColors[2];
            for (int i = 0; i < 2; ++i)
            {
                const int block = readWord(page + 1 + 4 * (count - 2 + i), 4);
                buttons[i] = {static_cast<int16_t>(readWord(block, 2)),
                              static_cast<int16_t>(readWord(block + 2, 2)),
                              static_cast<int>(readWord(block + 4, 1)),
                              static_cast<int>(readWord(block + 5, 1))};
                topColors[i] = readWord(block + (i == next ? 8 : 16), 2);
            }
            const auto inButton = [&](const Adafruit_ST7735::Window &write) {
                for (const auto &button : buttons)
                    if (write.x >= button.x && write.y >= button.y &&
                        write.x + write.width <= button.x + button.width &&
                        write.y + write.height <= button.y + button.height) return true;
                return false;
            };
            for (size_t i = firstWindow; i < display.windows.size(); ++i)
                assert(inButton(display.windows[i]) && "TFT frame write outside confirmation buttons");
            for (size_t i = firstFill; i < display.filledRects.size(); ++i)
                assert(inButton(display.filledRects[i]) && "TFT fill outside confirmation buttons");
            for (int i = 0; i < 2; ++i)
            {
                const auto &button = buttons[i];
                const auto &other = buttons[1 - i];
                const int x = button.x + button.width / 2, y = button.y;
                const bool overlaps = x >= other.x && x < other.x + other.width &&
                                      y >= other.y && y < other.y + other.height;
                if (x >= 0 && x < 128 && y >= 0 && y < 160 && !overlaps)
                    assert(display.screen[y * 128 + x] == topColors[i]);
            }
        }
        else
        {
            assert(display.windows.size() == firstWindow + 1);
            assert(display.windows.back().y == 144 && display.windows.back().height == 16);
        }
        matchesFullRedraw();
    };
    const auto checkNoRedraw = [&](bool next) {
        const size_t windows = display.windows.size(), fills = display.filledRects.size();
        if (next) game.OnRightKey();
        else game.OnLeftKey();
        assert(display.windows.size() == windows && display.filledRects.size() == fills);
    };
    size_t beforeMove = display.windows.size();
    constexpr char sharedPath[] = "/assets/shared.data";
    if (banded && !list)
    {
        // The destination's full image remains available for a later redraw.
        // A partial selection must not read any of its compressed payload.
        AssetData::OpenFrame target;
        assert(resources.frameReader().openFrame({0, 0, art, 0, static_cast<uint16_t>(16 + 2 * (items + products))}, target));
        assert(target.descriptor.height == 160);
        sd.failReadAt(sharedPath, target.descriptor.payloadOffset);
        target.file.close();
    }
    game.OnRightKey(); // Product to inventory entry, with both rows in view in list mode.
    sd.failReadAt(sharedPath, static_cast<size_t>(-1));
    if (list)
    {
        assert(display.windows.size() == beforeMove + 2);
        for (size_t i = beforeMove; i < display.windows.size(); ++i)
            assert(display.windows[i].height == 16 && display.windows[i].width == (styledBrowse ? 60 : 120));
        if (styledBrowse)
        {
            assert(display.screen[88 * 128 + 4] != 0xf000);
            assert(display.screen[88 * 128 + 64] == 0xf000);
        }
    }
    else
    {
        assert(display.windows.size() > beforeMove);
        for (size_t i = beforeMove; i < display.windows.size(); ++i)
            assert(banded ? display.windows[i].height <= 16 : display.windows[i].height < 160);
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
    const auto returning = display.screen;
    checkButtons(false, false);
    checkNoRedraw(false);
    checkButtons(false, true);
    assert(display.screen == returning);
    checkNoRedraw(true);
    checkButtons(false, false);
    game.OnConfirmKey();
    assert(pet.customStat(0) == 43 && pet.persistentState().shopQuantities[0] == 2);
    if (styledBrowse)
    {
        assert(display.screen[136 * 128] == 0xF716); // The configured full-screen receipt footer starts here.
        assert(display.screen[0] == 0x0010);
    }
    matchesFullRedraw();
    game.OnConfirmKey(); // Receipt to shop
    game.OnRightKey();
    game.OnConfirmKey(); // Inventory
    matchesFullRedraw();
    game.OnConfirmKey(); // Use confirmation
    checkNoRedraw(true);
    checkButtons(true, false);
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
