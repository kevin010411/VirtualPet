#include "app/Game.h"

#include <Arduino.h>
#include <string.h>
#include "domain/Pet.h"
#include "storage/PetStorage.h"
#include "render/Renderer.h"

Game::Game(Pet &petRef, PetStorage &petStorageRef, Renderer &rendererRef, AppearanceLoader &appearanceLoaderRef)
    : pet(petRef),
      petStorage(petStorageRef),
      renderer(rendererRef),
      appearanceLoader(appearanceLoaderRef),
      commands(*this)
#if ENABLE_GUESS_ITEM_GAME
      ,
      guessItem(new GuessItemGame(*this))
#endif
{
}

Game::~Game()
{
#if ENABLE_GUESS_ITEM_GAME
    delete guessItem;
#endif
}

void Game::setRendererAssetAppearance(const char *speciesCode, const char *outfitCode)
{
    if (pet.setSpeciesCode(speciesCode))
        strncpy(defaultSpeciesCode, pet.speciesCode(), sizeof(defaultSpeciesCode) - 1);
    defaultSpeciesCode[sizeof(defaultSpeciesCode) - 1] = '\0';

    if (pet.setOutfitCode(outfitCode))
        strncpy(defaultOutfitCode, pet.outfitCode(), sizeof(defaultOutfitCode) - 1);
    defaultOutfitCode[sizeof(defaultOutfitCode) - 1] = '\0';

    renderer.setAssetAppearance(defaultSpeciesCode, defaultOutfitCode);
    renderer.reloadManifest();
    refreshBaseAnimation();
    dirtyAnimation = true;
}

bool Game::saveNow()
{
    const bool saved = petStorage.save(pet);
    if (saved)
        saveCounter = 0;
    return saved;
}

void Game::startBatteryAnimation()
{
    clearAnimationsByOwner(AnimationOwner::Command);
    clearAnimationsByOwner(AnimationOwner::Minigame);
    clearAnimationsByOwner(AnimationOwner::System);
    showAnimationId = AnimationId::Battery;
    frameInterval = renderer.frameIntervalFor(showAnimationId, frameIntervalSlow);
    lastFrameTime = 0;
    animateDone = !renderer.setAnimation(showAnimationId, false);
    if (!animateDone)
        animateDone = renderer.advanceAnimationFrame();
}

void Game::updateBatteryAnimation(unsigned long now)
{
    if (now - lastFrameTime < frameInterval)
        return;

    lastFrameTime = now;
    renderer.advanceAnimationFrame();
}

AnimationId Game::fortuneToAnimationId(int fortuneIndex)
{
    switch (fortuneIndex)
    {
    case 1:
        return AnimationId::Predict1;
    case 2:
        return AnimationId::Predict2;
    case 3:
        return AnimationId::Predict3;
    case 4:
        return AnimationId::Predict4;
    case 5:
        return AnimationId::Predict5;
    case 6:
        return AnimationId::Predict6;
    case 7:
        return AnimationId::Predict7;
    case 8:
        return AnimationId::Predict8;
    case 9:
        return AnimationId::Predict9;
    case 10:
        return AnimationId::Predict10;
    default:
        return AnimationId::Predict11;
    }
}

void Game::setup_game()
{
    renderer.initAnimations();
    commands.resetSelection();
    baseAnimationId = currentBaseAnimation();
    renderer.setAnimation(baseAnimationId, false);
    draw_all_layout();

    dirtySelect = true;
    dirtyAnimation = true;
    animateDone = true;
    displayDuration = 0;
    saveCounter = 0;
    environmentCooldown = 0;
    showAnimationId = AnimationId::None;
    hasActiveAnimation = false;
    activeAnimation = Animation();
    animationQueue.clear();
    exitOutfitSelection();
#if ENABLE_GUESS_ITEM_GAME
    guessItem->reset();
#endif
    last_tick_time = millis();

    if (!petStorage.load(pet))
    {
        pet.setDefaultState();
        pet.setSpeciesCode(defaultSpeciesCode);
        pet.setOutfitCode(defaultOutfitCode);
    }

    applySpeciesForHealthyDays();
    renderer.setAssetAppearance(pet.speciesCode(), pet.outfitCode());
    renderer.reloadManifest();
    refreshBaseAnimation();
}

void Game::loop_game()
{
    const unsigned long current_time = millis();

    const unsigned long elapsed = current_time - last_tick_time;
    if (elapsed >= gameTick)
    {
        last_tick_time = current_time;
        maybeDecayEnvironment(elapsed);
        ControlAnimation(elapsed);
#if ENABLE_GUESS_ITEM_GAME
        guessItem->update();
#endif

        if (animationQueue.empty())
        {
            roll_sick();
            if (pet.dayPassed())
                applySpeciesForHealthyDays();
        }

        refreshBaseAnimation();
        maybeSavePet();
    }
    else
    {
#if ENABLE_GUESS_ITEM_GAME
        guessItem->update();
#endif
    }

    RenderGame(current_time);
}

void Game::requestFullRedraw()
{
    dirtySelect = true;
    dirtyAnimation = true;
    showAnimationId = AnimationId::None;
    animateDone = true;
    lastFrameTime = 0;
}

void Game::maybeDecayEnvironment(unsigned long elapsed)
{
    environmentCooldown -= static_cast<long>(elapsed);
    if (environmentCooldown > 0)
        return;

    pet.decayEnvironment(environmentDecayAmount);
    environmentCooldown = random(3 * gameTick, 6 * gameTick);
}

void Game::maybeSavePet()
{
    saveCounter += 1;
    if (saveCounter < savePeriodTicks)
        return;

    if (petStorage.save(pet))
        saveCounter = 0;
}

AnimationId Game::currentBaseAnimation() const
{
    const AnimationId candidateAnimation = pet.CurrentAnimation();
    if (hasAnimation(candidateAnimation))
        return candidateAnimation;

    if (candidateAnimation != AnimationId::Idle && hasAnimation(AnimationId::Idle))
        return AnimationId::Idle;

    return candidateAnimation;
}

void Game::refreshBaseAnimation()
{
    const AnimationId candidateAnimation = currentBaseAnimation();
    if (baseAnimationId != candidateAnimation)
    {
        baseAnimationId = candidateAnimation;
        dirtyAnimation = true;
    }
}

void Game::ControlAnimation(unsigned long elapsed)
{
    if (!hasActiveAnimation)
        return;

    displayDuration -= static_cast<long>(elapsed);
    if (displayDuration > 0 && activeAnimation.isFixedFrame())
        return;

    if (displayDuration > 0 && !animateDone)
        return;

    hasActiveAnimation = false;
    dirtyAnimation = true;
    animateDone = false;
}

void Game::tryStartNextAnimation()
{
    if (hasActiveAnimation || animationQueue.empty())
        return;

    activeAnimation = animationQueue.front();
    animationQueue.pop_front();
    hasActiveAnimation = true;
    displayDuration = static_cast<long>(activeAnimation.durationMs);
}

void Game::RenderGame(unsigned long current_time)
{
    if (selectingOutfit)
    {
        renderOutfitPreview(current_time);
        if (dirtySelect)
        {
            draw_select_layout();
            dirtySelect = false;
        }
        return;
    }

    const bool frame_due = (current_time - lastFrameTime >= frameInterval);
    if (frame_due)
        lastFrameTime = current_time;

    if (dirtyAnimation || showAnimationId == AnimationId::None)
    {
        bool playOnce = false;
        tryStartNextAnimation();
        if (hasActiveAnimation)
        {
            showAnimationId = activeAnimation.id;
            playOnce = activeAnimation.playOnce;
        }
        else
        {
            showAnimationId = baseAnimationId;
        }

        frameInterval = renderer.frameIntervalFor(showAnimationId, frameIntervalSlow);
        if (hasActiveAnimation && activeAnimation.isFixedFrame())
        {
            animateDone = !renderer.ShowAnimationFrame(showAnimationId, activeAnimation.frameIndex);
        }
        else
        {
            animateDone = !renderer.setAnimation(showAnimationId, playOnce);
            if (!animateDone)
                animateDone = renderer.advanceAnimationFrame();
        }
        dirtyAnimation = false;
    }
    else if (frame_due && !(hasActiveAnimation && activeAnimation.isFixedFrame()))
    {
        animateDone |= renderer.advanceAnimationFrame();
    }

    if (dirtySelect)
    {
        draw_select_layout();
        dirtySelect = false;
    }
}

void Game::resetPet()
{
    pet.setDefaultState();
    applySpeciesForHealthyDays();
    renderer.setAssetAppearance(pet.speciesCode(), pet.outfitCode());
    renderer.reloadManifest();
    clearAnimationsByOwner(AnimationOwner::Command);
    clearAnimationsByOwner(AnimationOwner::Minigame);
    refreshBaseAnimation();
    dirtyAnimation = true;
    saveCounter = 0;
    petStorage.save(pet);
}

bool Game::applySpeciesForHealthyDays()
{
    AppearanceSelection selection = {};
    if (!appearanceLoader.findForHealthyDays(pet.healthyDays(), selection))
        return false;

    if (strcmp(pet.speciesCode(), selection.speciesCode) == 0)
        return false;

    return applyAppearance(selection.speciesCode, selection.outfitCode);
}

bool Game::applyAppearance(const char *speciesCode, const char *outfitCode)
{
    if (!pet.setSpeciesCode(speciesCode) || !pet.setOutfitCode(outfitCode))
        return false;

    renderer.setAssetAppearance(pet.speciesCode(), pet.outfitCode());
    renderer.reloadManifest();
    refreshBaseAnimation();
    dirtyAnimation = true;
    showAnimationId = AnimationId::None;
    lastFrameTime = 0;
    return petStorage.save(pet);
}

void Game::OnLeftKey()
{
    if (selectingOutfit)
    {
        changeOutfitSelection(-1);
        return;
    }

#if ENABLE_GUESS_ITEM_GAME
    if (guessItem->isActive())
    {
        guessItem->onLeft();
    }
    else
#endif
    {
        commands.next();
        dirtySelect = true;
    }
}

void Game::OnRightKey()
{
    if (selectingOutfit)
    {
        changeOutfitSelection(1);
        return;
    }

#if ENABLE_GUESS_ITEM_GAME
    if (guessItem->isActive())
    {
        guessItem->onRight();
    }
    else
#endif
    {
        commands.prev();
        dirtySelect = true;
    }
}

void Game::OnConfirmKey()
{
    if (selectingOutfit)
    {
        confirmOutfitSelection();
        return;
    }

#if ENABLE_GUESS_ITEM_GAME
    if (guessItem->isActive())
    {
        guessItem->onMid();
    }
    else
#endif
    {
        commands.executeCurrent();
    }
}

bool Game::hasAnimation(AnimationId id) const
{
    return renderer.frameCountFor(id) > 0;
}

bool Game::hasAnimations(const AnimationId *ids, size_t count) const
{
    if (ids == nullptr)
        return false;

    for (size_t i = 0; i < count; ++i)
    {
        if (!hasAnimation(ids[i]))
            return false;
    }
    return true;
}

bool Game::canPlayGuessItemGame() const
{
#if ENABLE_GUESS_ITEM_GAME
    const AnimationId required[] = {
        AnimationId::GuessItem1,
        AnimationId::GuessItem2,
        AnimationId::GuessItem3,
        AnimationId::GuessItem4,
        AnimationId::GuessLL,
        AnimationId::GuessLR,
        AnimationId::GuessRL,
        AnimationId::GuessRR};
    return hasAnimations(required, sizeof(required) / sizeof(required[0]));
#else
    return false;
#endif
}

void Game::queueAnimation(const Animation &animation)
{
    if (animation.id != AnimationId::None && !hasAnimation(animation.id))
        return;

    auto insertIt = animationQueue.end();
    for (auto it = animationQueue.begin(); it != animationQueue.end(); ++it)
    {
        if (static_cast<uint8_t>(animation.priority) > static_cast<uint8_t>(it->priority))
        {
            insertIt = it;
            break;
        }
    }
    animationQueue.insert(insertIt, animation);
}

void Game::queuePostCommandHappyAnimation()
{
    if (!hasAnimation(AnimationId::Happy))
        return;

    queueAnimation(Animation(AnimationId::Happy, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
}

void Game::queueGiftAnimation()
{
    pet.changeMood(50);
    queueAnimation(Animation(AnimationId::Gift, gameTick * 2.5, false, AnimationOwner::Command, AnimationPriority::High));

    if (hasAnimation(AnimationId::GiftHappy))
        queueAnimation(Animation(AnimationId::GiftHappy, gameTick * 1.5, false, AnimationOwner::Command, AnimationPriority::High));

    queuePostCommandHappyAnimation();
    markAnimationDirty();
}

void Game::clearAnimationsByOwner(AnimationOwner owner)
{
    for (auto it = animationQueue.begin(); it != animationQueue.end();)
    {
        if (it->owner == owner)
            it = animationQueue.erase(it);
        else
            ++it;
    }

    if (hasActiveAnimation && activeAnimation.owner == owner)
    {
        hasActiveAnimation = false;
        activeAnimation = Animation();
        displayDuration = 0;
        animateDone = true;
        showAnimationId = AnimationId::None;
    }
}

bool Game::hasAnimationForOwner(AnimationOwner owner) const
{
    if (hasActiveAnimation && activeAnimation.owner == owner)
        return true;

    for (const Animation &animation : animationQueue)
    {
        if (animation.owner == owner)
            return true;
    }

    return false;
}

void Game::markAnimationDirty()
{
    dirtyAnimation = true;
}

void Game::changePetMood(int delta)
{
    pet.changeMood(delta);
}

bool Game::commandHasAnimation(AnimationId id) const
{
    return hasAnimation(id);
}

AnimationId Game::commandCurrentAgeAnimation() const
{
    return pet.CurrentAgeAnimation();
}

void Game::commandClearCommandAnimations()
{
    clearAnimationsByOwner(AnimationOwner::Command);
}

void Game::commandFeedPet()
{
    pet.feedPet(40);
    queueAnimation(Animation(AnimationId::Feed, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
    queuePostCommandHappyAnimation();
    markAnimationDirty();
}

void Game::commandPredict()
{
    queueAnimation(Animation(AnimationId::PredAnim, gameTick * 20, true, AnimationOwner::Command, AnimationPriority::High));
    roll_fortune();
    markAnimationDirty();
}

void Game::commandGift()
{
    queueGiftAnimation();
}

void Game::commandMedicine()
{
    pet.takeMedicine();
    queueAnimation(Animation(AnimationId::Heal, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
    queuePostCommandHappyAnimation();
    markAnimationDirty();
}

void Game::commandShower()
{
    pet.takeShower(250);
    queueAnimation(Animation(AnimationId::Shower, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
    queuePostCommandHappyAnimation();
    markAnimationDirty();
}

#if ENABLE_GUESS_ITEM_GAME
void Game::commandHaveFun()
{
    clearAnimationsByOwner(AnimationOwner::Command);
    if (canPlayGuessItemGame())
        guessItem->start();
    else
        queueGiftAnimation();
}
#endif

void Game::commandClean()
{
    pet.cleanEnv(500);
    queueAnimation(Animation(AnimationId::Clean, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
    queuePostCommandHappyAnimation();
    markAnimationDirty();
}

void Game::commandChangeOutfit()
{
    if (!loadOutfitOptions())
        return;

    selectingOutfit = true;
    clearAnimationsByOwner(AnimationOwner::Command);
    hasActiveAnimation = false;
    activeAnimation = Animation();
    showAnimationId = AnimationId::None;
    animateDone = true;
    dirtyAnimation = false;
    outfitPreviewFrame = 1;
    lastOutfitPreviewFrameTime = 0;
    dirtyOutfitPreview = true;
    loadSelectedOutfitPreview();
}

void Game::commandStatus()
{
    queueStatusAnimation();
}

void Game::queueStatusAnimation()
{
#if APP_STATUS_MODE == STATUS_MODE_STATUS
    queueStatusDirectAnimation();
#elif APP_STATUS_MODE == STATUS_MODE_RANDOM3
    if (!queueStatusRandom3Animation())
        queueStatusAgeAnimation();
#else
    queueStatusAgeAnimation();
#endif
}

bool Game::queueStatusDirectAnimation()
{
    if (!hasAnimation(AnimationId::Status))
        return false;

    queueAnimation(Animation(AnimationId::Status, gameTick * 10, true, AnimationOwner::Command, AnimationPriority::Normal));
    markAnimationDirty();
    return true;
}

bool Game::queueStatusAgeAnimation()
{
    const AnimationId ageAnimation = pet.CurrentAgeAnimation();
    const uint16_t maxFrame = renderer.frameCountFor(ageAnimation);
    if (maxFrame == 0)
        return false;

    queueAnimation(Animation(ageAnimation, gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal, pet.CurrentAgeFrame(maxFrame)));
    markAnimationDirty();
    return true;
}

bool Game::queueStatusRandom3Animation()
{
    AnimationId candidates[3] = {
        AnimationId::StatusAge,
        AnimationId::StatusHappy,
        AnimationId::StatusHungry,
    };

    const uint8_t start = random(3);
    for (uint8_t attempts = 0; attempts < 3; ++attempts)
    {
        const uint8_t index = (start + attempts) % 3;
        const AnimationId chosen = candidates[index];
        if (!hasAnimation(chosen))
            continue;

        const uint16_t maxFrame = renderer.frameCountFor(chosen);
        if (maxFrame == 0)
            continue;

        uint16_t frame = 1;
        if (chosen == AnimationId::StatusAge)
            frame = pet.CurrentAgeFrame(maxFrame);
        else if (chosen == AnimationId::StatusHappy)
            frame = pet.CurrentMoodFrame(maxFrame);
        else if (chosen == AnimationId::StatusHungry)
            frame = pet.CurrentHungerFrame(maxFrame);
        else
            continue;

        queueAnimation(Animation(chosen, gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal, frame));
        markAnimationDirty();
        return true;
    }

    return false;
}

bool Game::loadOutfitOptions()
{
    outfitOptionCount = 0;
    selectedOutfitIndex = 0;
    hasSelectedOutfitPreview = false;
    if (!appearanceLoader.loadOutfits(pet.speciesCode(), outfitOptions, maxOutfitOptions, outfitOptionCount))
        return false;

    for (size_t i = 0; i < outfitOptionCount; ++i)
    {
        if (strcmp(outfitOptions[i], pet.outfitCode()) == 0)
        {
            selectedOutfitIndex = i;
            break;
        }
    }

    return outfitOptionCount > 0;
}

bool Game::loadSelectedOutfitPreview()
{
    hasSelectedOutfitPreview = false;
    selectedOutfitPreview = {};
    outfitPreviewFrame = 1;
    if (selectedOutfitIndex >= outfitOptionCount)
        return false;

    hasSelectedOutfitPreview = appearanceLoader.findOutfitPreview(pet.speciesCode(), outfitOptions[selectedOutfitIndex], selectedOutfitPreview);
    if (hasSelectedOutfitPreview)
    {
        outfitPreviewInterval = selectedOutfitPreview.frameIntervalMs == 0
                                    ? frameIntervalSlow
                                    : max(1UL, static_cast<unsigned long>(selectedOutfitPreview.frameIntervalMs));
    }
    else
    {
        outfitPreviewInterval = frameIntervalSlow;
    }
    dirtyOutfitPreview = true;
    return hasSelectedOutfitPreview;
}

void Game::changeOutfitSelection(int delta)
{
    if (!selectingOutfit || outfitOptionCount == 0)
        return;

    if (delta < 0)
        selectedOutfitIndex = (selectedOutfitIndex == 0) ? (outfitOptionCount - 1) : (selectedOutfitIndex - 1);
    else
        selectedOutfitIndex = (selectedOutfitIndex + 1) % outfitOptionCount;

    loadSelectedOutfitPreview();
    lastOutfitPreviewFrameTime = 0;
}

void Game::renderOutfitPreview(unsigned long now)
{
    if (!hasSelectedOutfitPreview)
        return;

    const bool frameDue = dirtyOutfitPreview || lastOutfitPreviewFrameTime == 0 || now - lastOutfitPreviewFrameTime >= outfitPreviewInterval;
    if (!frameDue)
        return;

    lastOutfitPreviewFrameTime = now;
    renderer.ShowSDCardFrame(selectedOutfitPreview.path, outfitPreviewFrame, 0, 32);
    dirtyOutfitPreview = false;

    ++outfitPreviewFrame;
    if (outfitPreviewFrame > selectedOutfitPreview.frameCount)
        outfitPreviewFrame = 1;
}

void Game::confirmOutfitSelection()
{
    if (outfitOptionCount == 0 || selectedOutfitIndex >= outfitOptionCount)
    {
        exitOutfitSelection();
        return;
    }

    char selectedOutfit[9] = {};
    strncpy(selectedOutfit, outfitOptions[selectedOutfitIndex], sizeof(selectedOutfit) - 1);
    exitOutfitSelection();
    applyAppearance(pet.speciesCode(), selectedOutfit);
}

void Game::exitOutfitSelection()
{
    selectingOutfit = false;
    outfitOptionCount = 0;
    selectedOutfitIndex = 0;
    hasSelectedOutfitPreview = false;
    selectedOutfitPreview = {};
    outfitPreviewFrame = 1;
    outfitPreviewInterval = frameIntervalSlow;
    lastOutfitPreviewFrameTime = 0;
    dirtyOutfitPreview = false;
    dirtyAnimation = true;
    showAnimationId = AnimationId::None;
}

void Game::draw_all_layout()
{
    for (int i = 0; i < commands.commandCount(); ++i)
    {
        int y_start = 0;
        if (i >= 4)
            y_start = 160 - 32;

        renderer.ShowSDCardFrame("/layout", static_cast<uint16_t>(i + 1), (i % 4) * 32, y_start);
    }
}

void Game::draw_select_layout()
{
    const int prevIdx = commands.previousSlot();
    if (commands.isSlotVisible(prevIdx))
    {
        const int y_prev = (prevIdx >= 4) ? (160 - 32) : 0;
        renderer.ShowSDCardFrame("/layout", static_cast<uint16_t>(prevIdx + 1), (prevIdx % 4) * 32, y_prev);
    }

    const int curIdx = commands.selectedSlot();
    if (!commands.isSlotVisible(curIdx))
        return;

    const int y_cur = (curIdx >= 4) ? (160 - 32) : 0;
    renderer.ShowSDCardFrame("/layout_sel", static_cast<uint16_t>(curIdx + 1), (curIdx % 4) * 32, y_cur);
}

void Game::roll_sick()
{
    const float probability = 0.001f;
    const float randValue = random(1001) / 1000.0f;
    if (randValue < probability)
        pet.getSick();
}

void Game::roll_fortune()
{
    queueAnimation(Animation(fortuneToAnimationId(random(1, maxFortune + 1)), gameTick * 2.4, false, AnimationOwner::Command, AnimationPriority::High));
}
