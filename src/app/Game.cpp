#include "app/Game.h"

#include <Arduino.h>
#include <Adafruit_ST7735.h>
#include <SdFat.h>
#include <string.h>
#include "domain/Pet.h"
#include "storage/PetStorage.h"
#include "render/Renderer.h"

namespace
{
constexpr const char *kSpeciesByHealthyDaysPath = "/species_by_healthy_days.txt";
constexpr const char *kCommandRulesPath = "/command_rules.txt";
constexpr uint32_t kNoMaxHealthyDays = 0xFFFFFFFFUL;

bool isSpaceChar(char c)
{
    return c == ' ' || c == '\t';
}

char *trimField(char *text)
{
    while (text != nullptr && isSpaceChar(*text))
        ++text;

    if (text == nullptr || *text == '\0')
        return text;

    char *end = text + strlen(text) - 1;
    while (end >= text && isSpaceChar(*end))
    {
        *end = '\0';
        --end;
    }
    return text;
}

bool readConfigLine(File &file, char *line, size_t lineSize)
{
    if (line == nullptr || lineSize == 0)
        return false;

    size_t index = 0;
    bool sawAny = false;
    while (file.available())
    {
        const char c = static_cast<char>(file.read());
        sawAny = true;
        if (c == '\r')
            continue;
        if (c == '\n')
            break;
        if (index + 1 < lineSize)
            line[index++] = c;
    }

    line[index] = '\0';
    return sawAny || index > 0;
}

bool parseUnsignedField(const char *text, uint32_t &value)
{
    if (text == nullptr || text[0] == '\0')
        return false;

    uint32_t parsed = 0;
    for (const char *cursor = text; *cursor != '\0'; ++cursor)
    {
        if (*cursor < '0' || *cursor > '9')
            return false;
        parsed = parsed * 10UL + static_cast<uint32_t>(*cursor - '0');
    }

    value = parsed;
    return true;
}

bool parseRangeField(const char *text, uint32_t &value)
{
    if (strcmp(text, "*") == 0)
    {
        value = kNoMaxHealthyDays;
        return true;
    }

    return parseUnsignedField(text, value);
}

bool parseEnabledField(const char *text, bool &enabled)
{
    if (strcmp(text, "1") == 0)
    {
        enabled = true;
        return true;
    }

    if (strcmp(text, "0") == 0)
    {
        enabled = false;
        return true;
    }

    return false;
}

bool isValidAppearanceCode(const char *code)
{
    if (code == nullptr || code[0] == '\0')
        return false;

    const size_t len = strlen(code);
    if (len > 8)
        return false;

    for (size_t i = 0; i < len; ++i)
    {
        const char c = code[i];
        const bool valid = (c >= 'a' && c <= 'z') ||
                           (c >= 'A' && c <= 'Z') ||
                           (c >= '0' && c <= '9') ||
                           c == '_' || c == '-';
        if (!valid)
            return false;
    }

    return true;
}

bool splitSpeciesRule(char *line, uint32_t &minDays, uint32_t &maxDays, char *&speciesCode, char *&outfitCode)
{
    char *fields[4] = {};
    fields[0] = line;
    int fieldIndex = 1;

    for (char *cursor = line; *cursor != '\0'; ++cursor)
    {
        if (*cursor != '|')
            continue;
        if (fieldIndex >= 4)
            return false;
        *cursor = '\0';
        fields[fieldIndex++] = cursor + 1;
    }

    if (fieldIndex != 4)
        return false;

    for (int i = 0; i < 4; ++i)
        fields[i] = trimField(fields[i]);

    if (!parseUnsignedField(fields[0], minDays))
        return false;

    if (strcmp(fields[1], "*") == 0)
        maxDays = kNoMaxHealthyDays;
    else if (!parseUnsignedField(fields[1], maxDays))
        return false;

    speciesCode = fields[2];
    outfitCode = fields[3];
    return minDays <= maxDays &&
           isValidAppearanceCode(speciesCode) &&
           isValidAppearanceCode(outfitCode);
}

bool splitCommandRule(char *line, char *fields[], size_t fieldCount)
{
    if (line == nullptr || fields == nullptr || fieldCount == 0)
        return false;

    fields[0] = line;
    size_t fieldIndex = 1;
    for (char *cursor = line; *cursor != '\0'; ++cursor)
    {
        if (*cursor != '|')
            continue;
        if (fieldIndex >= fieldCount)
            return false;
        *cursor = '\0';
        fields[fieldIndex++] = cursor + 1;
    }

    if (fieldIndex != fieldCount)
        return false;

    for (size_t i = 0; i < fieldCount; ++i)
        fields[i] = trimField(fields[i]);

    return true;
}

} // namespace

Game::Game(Adafruit_ST7735 *ref_tft, SdFat *ref_SD)
    : pet(new Pet()),
      petStorage(new PetStorage(ref_SD)),
      renderer(Renderer::create(ref_tft, ref_SD)),
      guessItem(new GuessItemGame(*this)),
      sd(ref_SD)
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

    applySpeciesForHealthyDays();
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
            if (pet->dayPassed())
                applySpeciesForHealthyDays();
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
        if (hasActiveAnimation && activeAnimation.isFixedFrame())
        {
            animateDone = !renderer->ShowAnimationFrame(showAnimationId, activeAnimation.frameIndex);
        }
        else
        {
            animateDone = !renderer->setAnimation(showAnimationId, playOnce);
            if (!animateDone)
                animateDone = renderer->advanceAnimationFrame();
        }
        dirtyAnimation = false;
    }
    else if (frame_due && !(hasActiveAnimation && activeAnimation.isFixedFrame()))
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
    applySpeciesForHealthyDays();
    renderer->setAssetAppearance(pet->speciesCode(), pet->outfitCode());
    renderer->reloadManifest();
    clearAnimationsByOwner(AnimationOwner::Command);
    clearAnimationsByOwner(AnimationOwner::Minigame);
    refreshBaseAnimation();
    dirtyAnimation = true;
    saveCounter = 0;
    petStorage->save(*pet);
}

bool Game::applySpeciesForHealthyDays()
{
    if (sd == nullptr || !sd->exists(kSpeciesByHealthyDaysPath))
        return false;

    File file = sd->open(kSpeciesByHealthyDaysPath, FILE_READ);
    if (!file)
        return false;

    const uint32_t healthyDays = pet->healthyDays();
    char line[80] = {};
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        uint32_t minDays = 0;
        uint32_t maxDays = 0;
        char *speciesCode = nullptr;
        char *outfitCode = nullptr;
        if (!splitSpeciesRule(content, minDays, maxDays, speciesCode, outfitCode))
            continue;

        if (healthyDays < minDays || healthyDays > maxDays)
            continue;

        file.close();
        if (strcmp(pet->speciesCode(), speciesCode) == 0 &&
            strcmp(pet->outfitCode(), outfitCode) == 0)
            return false;

        return applyAppearance(speciesCode, outfitCode);
    }

    file.close();
    return false;
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
    if (!isCommandEnabled(nowCommand))
        return;

    parse_command(nowCommand);
}

bool Game::commandMatches(Command command, const char *text) const
{
    if (text == nullptr)
        return false;

    if (strcmp(text, "*") == 0 || strcmp(text, commandLabel(command)) == 0)
        return true;

    switch (command)
    {
    case Command::FeedPet:
        return strcmp(text, "FeedPet") == 0;
    case Command::Predict:
        return strcmp(text, "Predict") == 0;
    case Command::Gift:
        return strcmp(text, "Gift") == 0;
    case Command::Medicine:
        return strcmp(text, "Medicine") == 0;
    case Command::Shower:
        return strcmp(text, "Shower") == 0;
    case Command::HaveFun:
        return strcmp(text, "HaveFun") == 0;
    case Command::Clean:
        return strcmp(text, "Clean") == 0;
    case Command::Status:
        return strcmp(text, "Status") == 0;
    case Command::Count:
        break;
    }

    return false;
}

bool Game::isCommandEnabled(Command command)
{
    if (sd == nullptr || !sd->exists(kCommandRulesPath))
        return true;

    File file = sd->open(kCommandRulesPath, FILE_READ);
    if (!file)
        return true;

    char line[96] = {};
    while (readConfigLine(file, line, sizeof(line)))
    {
        char *content = trimField(line);
        if (content == nullptr || content[0] == '\0' || content[0] == '#')
            continue;

        char *fields[4] = {};
        if (!splitCommandRule(content, fields, 4))
            continue;

        bool enabled = true;
        uint32_t minDays = 0;
        uint32_t maxDays = 0;
        if (!commandMatches(command, fields[0]) ||
            !parseEnabledField(fields[1], enabled) ||
            !parseUnsignedField(fields[2], minDays) ||
            !parseRangeField(fields[3], maxDays) ||
            minDays > maxDays)
        {
            continue;
        }

        const uint32_t healthyDays = pet->healthyDays();
        if (healthyDays < minDays || healthyDays > maxDays)
            continue;

        file.close();
        return enabled;
    }

    file.close();
    return true;
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
    {
        const AnimationId ageAnimation = pet->CurrentAgeAnimation();
        const uint16_t maxFrame = renderer->frameCountFor(ageAnimation);
        queueAnimation(Animation(ageAnimation, gameTick * 4, false, AnimationOwner::Command, AnimationPriority::Normal, pet->CurrentAgeFrame(maxFrame)));
        markAnimationDirty();
        break;
    }
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
