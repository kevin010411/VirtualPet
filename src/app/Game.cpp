#include "app/Game.h"

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <string.h>
#include "domain/Pet.h"
#include "storage/PetStorage.h"
#include "render/Renderer.h"

Game::Game(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
    : pet(new Pet()),
      petStorage(new PetStorage(ref_SD)),
      renderer(Renderer::create(ref_tft, ref_SD)),
      guessItem(new GuessItemGame(*this))
{
}

Game::~Game()
{
    delete guessItem;
    delete renderer;
    delete petStorage;
    delete pet;
}

void Game::setRendererAssetAppearance(const char *speciesCode, const char *outfitCode)
{
    if (pet->setSpeciesCode(speciesCode))
        strncpy(defaultSpeciesCode, pet->speciesCode(), sizeof(defaultSpeciesCode) - 1);
    defaultSpeciesCode[sizeof(defaultSpeciesCode) - 1] = '\0';

    if (pet->setOutfitCode(outfitCode))
        strncpy(defaultOutfitCode, pet->outfitCode(), sizeof(defaultOutfitCode) - 1);
    defaultOutfitCode[sizeof(defaultOutfitCode) - 1] = '\0';

    renderer->setAssetAppearance(defaultSpeciesCode, defaultOutfitCode);
    renderer->reloadManifest();
    dirtyAnimation = true;
}

bool Game::saveNow()
{
    const bool saved = petStorage->save(*pet);
    if (saved)
        saveCounter = 0;
    return saved;
}

bool Game::showBatteryScreen()
{
    return renderer->ShowSDCardFrame("/battery", 1,0,32);
}

void Game::startBatteryAnimation()
{
    clearAnimationsByOwner(AnimationOwner::Command);
    clearAnimationsByOwner(AnimationOwner::Minigame);
    clearAnimationsByOwner(AnimationOwner::System);
    showAnimationId = AnimationId::Battery;
    frameInterval = renderer->frameIntervalFor(showAnimationId, frameIntervalSlow);
    lastFrameTime = 0;
    animateDone = !renderer->setAnimation(showAnimationId, false);
    if (!animateDone)
        animateDone = renderer->advanceAnimationFrame();
}

void Game::updateBatteryAnimation(unsigned long now)
{
    if (now - lastFrameTime < frameInterval)
        return;

    lastFrameTime = now;
    renderer->advanceAnimationFrame();
}

const char *Game::commandLabel(Command command)
{
    switch (command)
    {
    case Command::FeedPet:
        return "FEED_PET";
    case Command::HaveFun:
        return "HAVE_FUN";
    case Command::Clean:
        return "CLEAN";
    case Command::Medicine:
        return "MEDICINE";
    case Command::Shower:
        return "SHOWER";
    case Command::Predict:
        return "PREDICT";
    case Command::Gift:
        return "GIFT";
    case Command::Status:
        return "STATUS";
    case Command::Count:
        break;
    }
    return "UNKNOWN";
}

int Game::commandCount()
{
    return static_cast<int>(Command::Count);
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
    renderer->initAnimations();
    lastSelected = static_cast<int>(nowCommand);
    baseAnimationId = pet->CurrentAnimation();
    renderer->setAnimation(baseAnimationId, false);
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
    guessItem->reset();
    last_tick_time = millis();

    if (!petStorage->load(*pet))
    {
        pet->setDefaultState();
        pet->setSpeciesCode(defaultSpeciesCode);
        pet->setOutfitCode(defaultOutfitCode);
    }

    renderer->setAssetAppearance(pet->speciesCode(), pet->outfitCode());
    renderer->reloadManifest();
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
        guessItem->update();

        if (animationQueue.empty())
        {
            roll_sick();
            pet->dayPassed();
        }

        refreshBaseAnimation();
        maybeSavePet();
    }
    else
    {
        guessItem->update();
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

    pet->decayEnvironment(environmentDecayAmount);
    environmentCooldown = random(3 * gameTick, 6 * gameTick);
}

void Game::maybeSavePet()
{
    saveCounter += 1;
    if (saveCounter < savePeriodTicks)
        return;

    if (petStorage->save(*pet))
        saveCounter = 0;
}

void Game::refreshBaseAnimation()
{
    const AnimationId candidateAnimation = pet->CurrentAnimation();
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

        frameInterval = renderer->frameIntervalFor(showAnimationId, frameIntervalSlow);
        animateDone = !renderer->setAnimation(showAnimationId, playOnce);
        if (!animateDone)
            animateDone = renderer->advanceAnimationFrame();
        dirtyAnimation = false;
    }
    else if (frame_due)
    {
        animateDone |= renderer->advanceAnimationFrame();
    }

    if (dirtySelect)
    {
        draw_select_layout();
        dirtySelect = false;
    }
}

void Game::resetPet()
{
    pet->setDefaultState();
    renderer->setAssetAppearance(pet->speciesCode(), pet->outfitCode());
    renderer->reloadManifest();
    clearAnimationsByOwner(AnimationOwner::Command);
    clearAnimationsByOwner(AnimationOwner::Minigame);
    refreshBaseAnimation();
    dirtyAnimation = true;
    saveCounter = 0;
    petStorage->save(*pet);
}

bool Game::applyAppearance(const char *speciesCode, const char *outfitCode)
{
    if (!pet->setSpeciesCode(speciesCode) || !pet->setOutfitCode(outfitCode))
        return false;

    renderer->setAssetAppearance(pet->speciesCode(), pet->outfitCode());
    renderer->reloadManifest();
    dirtyAnimation = true;
    showAnimationId = AnimationId::None;
    lastFrameTime = 0;
    return petStorage->save(*pet);
}

void Game::OnLeftKey()
{
    if (guessItem->isActive())
        guessItem->onLeft();
    else
        NextCommand();
}

void Game::OnRightKey()
{
    if (guessItem->isActive())
        guessItem->onRight();
    else
        PrevCommand();
}

void Game::OnConfirmKey()
{
    if (guessItem->isActive())
        guessItem->onMid();
    else
        ExecuteCommand();
}

String Game::NowCommand()
{
    return commandLabel(nowCommand);
}

void Game::NextCommand()
{
    lastSelected = static_cast<int>(nowCommand);
    const int nextValue = (static_cast<int>(nowCommand) + 1) % commandCount();
    nowCommand = static_cast<Command>(nextValue);
    dirtySelect = true;
}

void Game::PrevCommand()
{
    lastSelected = static_cast<int>(nowCommand);
    const int currentValue = static_cast<int>(nowCommand);
    const int prevValue = (currentValue == 0) ? (commandCount() - 1) : (currentValue - 1);
    nowCommand = static_cast<Command>(prevValue);
    dirtySelect = true;
}

void Game::ExecuteCommand()
{
    parse_command(nowCommand);
}

void Game::queueAnimation(const Animation &animation)
{
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
    pet->changeMood(delta);
}

void Game::parse_command(Command command)
{
    switch (command)
    {
    case Command::FeedPet:
        pet->feedPet(40);
        queueAnimation(Animation(AnimationId::Feed, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
        queueAnimation(Animation(AnimationId::Happy, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
        markAnimationDirty();
        break;
    case Command::HaveFun:
        clearAnimationsByOwner(AnimationOwner::Command);
        guessItem->start();
        break;
    case Command::Clean:
        pet->cleanEnv(500);
        queueAnimation(Animation(AnimationId::Clean, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
        queueAnimation(Animation(AnimationId::Happy, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
        markAnimationDirty();
        break;
    case Command::Medicine:
        pet->takeMedicine();
        queueAnimation(Animation(AnimationId::Heal, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
        queueAnimation(Animation(AnimationId::Happy, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
        markAnimationDirty();
        break;
    case Command::Shower:
        pet->takeShower(250);
        queueAnimation(Animation(AnimationId::Shower, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
        queueAnimation(Animation(AnimationId::Happy, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
        markAnimationDirty();
        break;
    case Command::Predict:
        queueAnimation(Animation(AnimationId::PredAnim, gameTick * 20, true, AnimationOwner::Command, AnimationPriority::High));
        roll_fortune();
        markAnimationDirty();
        break;
    case Command::Gift:
        pet->changeMood(50);
        queueAnimation(Animation(AnimationId::Gift, gameTick * 2.5, false, AnimationOwner::Command, AnimationPriority::High));
        queueAnimation(Animation(AnimationId::GiftHappy, gameTick * 1.5, false, AnimationOwner::Command, AnimationPriority::High));
        queueAnimation(Animation(AnimationId::Happy, gameTick * 1.2, false, AnimationOwner::Command, AnimationPriority::High));
        markAnimationDirty();
        break;
    case Command::Status:
        queueAnimation(Animation(pet->CurrentAgeAnimation(), gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal));
        markAnimationDirty();
        break;
    case Command::Count:
        break;
    }
}

void Game::draw_all_layout()
{
    for (int i = 0; i < commandCount(); ++i)
    {
        int y_start = 0;
        if (i >= 4)
            y_start = 160 - 32;

        renderer->ShowSDCardFrame("/layout", static_cast<uint16_t>(i + 1), (i % 4) * 32, y_start);
    }
}

void Game::draw_select_layout()
{
    const int prevIdx = lastSelected;
    const int y_prev = (prevIdx >= 4) ? (160 - 32) : 0;
    renderer->ShowSDCardFrame("/layout", static_cast<uint16_t>(prevIdx + 1), (prevIdx % 4) * 32, y_prev);

    const int curIdx = static_cast<int>(nowCommand);
    const int y_cur = (curIdx >= 4) ? (160 - 32) : 0;
    renderer->ShowSDCardFrame("/layout_sel", static_cast<uint16_t>(curIdx + 1), (curIdx % 4) * 32, y_cur);
}

void Game::roll_sick()
{
    const float probability = 0.01f;
    const float randValue = random(1001) / 1000.0f;
    if (randValue < probability)
        pet->getSick();
}

void Game::roll_fortune()
{
    queueAnimation(Animation(fortuneToAnimationId(random(1, maxFortune + 1)), gameTick * 2.4, false, AnimationOwner::Command, AnimationPriority::High));
}
