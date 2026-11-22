#include "test_cases.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "appearance/SdAppearanceLoader.h"
#include "controller/CommandController.h"
#include "controller/Game.h"
#include "display/Renderer.h"
#include "pet/Pet.h"
#include "pet/PetStorage.h"
#include "resources/AssetResources.h"
#include "resources/RuntimeContractLoader.h"

void verifyBingbingEvents(const std::filesystem::path &root)
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
        RuntimeContract config = {};
        assert(loadRuntimeContract(&sd, pet.speciesSlot(), pet.outfitSlot(), config));
        CommandController buttons;
        buttons.configure(config.buttons, config.layout);
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
        RuntimeContract config = {};
        assert(loadRuntimeContract(&sd, pet.speciesSlot(), pet.outfitSlot(), config));
        CommandController buttons;
        buttons.configure(config.buttons, config.layout);
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
