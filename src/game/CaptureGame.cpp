#include "common/AppProfile.h"

#if ENABLE_CAPTURE_GAME
#include "game/CaptureGame.h"

#include <Arduino.h>
#include <stdlib.h>
#include "common/FirmwareRandom.h"
#include "display/Renderer.h"
#include "resources/AssetResources.h"

namespace
{
constexpr uint32_t frameIntervalMs = 50;
constexpr uint32_t fullFrame = (1UL << CaptureGame::bandCount) - 1UL;

int roundedPixel(int halfPixels)
{
    return halfPixels < 0 ? halfPixels / 2 : (halfPixels + 1) / 2;
}

uint32_t verticalBands(int from, int to)
{
    if (from < 0) from = 0;
    if (to > CaptureAssets::screenHeight) to = CaptureAssets::screenHeight;
    if (to <= from) return 0;
    const int first = from / CaptureGame::bandRows;
    const int end = (to + CaptureGame::bandRows - 1) / CaptureGame::bandRows;
    return ((1UL << end) - 1UL) ^ ((1UL << first) - 1UL);
}
}

bool CaptureGame::start(Renderer &renderer, AssetResources &resources, CaptureOutcomeHost &outcomes)
{
    cancel();
    if (!assets_.open(resources.sdCard())) return false;
    renderer_ = &renderer;
    outcomes_ = &outcomes;
    round_ = {};
    caughtTargets_ = 0;
    const auto &scene = assets_.scene();
    hookX_ = scene.hookX;
    score_ = 0;
    for (uint8_t index = 0; index < scene.density; ++index)
    {
        if (spawn(fish_[index], 0)) continue;
        cancel();
        return false;
    }
    state_ = State::Swimming;
    if (!draw(0, true))
    {
        cancel();
        return false;
    }
    started_ = millis();
    frameAt_ = 0;
    return true;
}

void CaptureGame::cancel()
{
    if (state_ != State::Closed || renderer_ != nullptr) assets_.close();
    renderer_ = nullptr;
    outcomes_ = nullptr;
    state_ = State::Closed;
}

bool CaptureGame::spawn(Fish &fish, uint32_t age)
{
    if (!assets_.pickTarget(FirmwareRandom::below(assets_.totalWeight()), fish.target)) return false;
    fish.born = age;
    fish.offsetX = FirmwareRandom::below(fish.target.width * 2);
    fish.offsetY = FirmwareRandom::below(fish.target.height * 2);
    const uint8_t direction = assets_.scene().direction;
    fish.reverse = direction == 1 || (direction == 0 && FirmwareRandom::below(2) == 0);
    return true;
}

CaptureGame::Position CaptureGame::position(const Fish &fish, uint32_t age) const
{
    const int travel = static_cast<int>((age - fish.born) * fish.target.speed / 1000UL);
    const int span = fish.target.width * 2;
    const int period = assets_.scene().direction == 0 ? span * 2 : span;
    int phase = (fish.offsetX + (fish.reverse ? -travel : travel)) % period;
    if (phase < 0) phase += period;
    const bool reflected = phase > span;
    return {
        2 * fish.target.x + (reflected ? period - phase : phase),
        2 * fish.target.y + fish.offsetY,
        reflected != fish.reverse,
    };
}

uint16_t CaptureGame::hookY(uint32_t dropAge) const
{
    const uint16_t start = assets_.scene().startY * 2;
    if (state_ != State::Drop || dropAge >= 2U * turnAt_) return start;
    const uint32_t progress = dropAge <= turnAt_ ? dropAge : 2U * turnAt_ - dropAge;
    return start + (hookTurnY_ - start) * progress / turnAt_;
}

void CaptureGame::traceDrop(uint32_t age)
{
    const auto &scene = assets_.scene();
    const uint16_t samples = (scene.depth - scene.startY) * 2;
    // Search at half-pixel positions along the same path used to draw the hook.
    for (uint16_t step = 0; step <= samples * 2U; ++step)
    {
        const uint16_t at = static_cast<uint32_t>(turnAt_) * step / samples;
        const int y = hookY(at);
        int nearest = 32767;

        for (uint8_t index = 0; index < scene.density; ++index)
        {
            const Fish &fish = fish_[index];
            const auto point = position(fish, age + at);
            const int dx = abs(point.x - 2 * hookX_);
            const int dy = abs(point.y - y);
            if (dx > fish.target.hitWidth || dy > fish.target.hitHeight) continue;
            const int distance = dx * dx + dy * dy;
            if (scene.overlap != 1 && distance >= nearest) continue;
            nearest = distance;
            caught_ = index;
        }

        if (caught_ < 0) continue;
        hitAt_ = at;
        if (at < turnAt_)
        {
            hookTurnY_ = y;
            turnAt_ = at == 0 ? 1 : at;
        }
        return;
    }
}

bool CaptureGame::draw(uint32_t age, bool full)
{
    const auto &scene = assets_.scene();
    const uint32_t dropAge = state_ == State::Drop ? age - dropStart_ : 0;
    const int hookTop = roundedPixel(hookY(dropAge));
    const int hookLeft = roundedPixel(2 * hookX_ - scene.hook.width);
    Frame frame = {};
    frame.hookX = hookX_;
    frame.lineStart = scene.startY;
    frame.lineEnd = hookTop;
    const uint32_t elapsedSeconds = age / 1000UL;
    frame.secondsLeft = elapsedSeconds < scene.durationSeconds ? scene.durationSeconds - elapsedSeconds : 0;
    for (uint8_t index = 0; index < scene.density; ++index)
    {
        const Fish &fish = fish_[index];
        const auto point = position(fish, age);
        const auto &sprite = fish.target.sprite;
        const bool attached = state_ == State::Drop && index == caught_ && dropAge >= hitAt_;
        const int x = attached ? 2 * hookX_ : point.x;
        const int y = attached ? 2 * (hookTop + scene.hook.height / 2) : point.y;
        frame.layers[frame.layerCount++] = {
            &sprite, static_cast<int16_t>(roundedPixel(x - sprite.width)),
            static_cast<int16_t>(roundedPixel(y - sprite.height)), point.movingLeft != fish.target.facesLeft,
        };
    }
    frame.layers[frame.layerCount++] = {&scene.hook, static_cast<int16_t>(hookLeft),
                                        static_cast<int16_t>(hookTop), false};

    uint32_t bands = verticalBands(scene.startY, hookTop);
    for (uint8_t index = 0; index < frame.layerCount; ++index)
    {
        const auto &layer = frame.layers[index];
        bands |= verticalBands(layer.y, layer.y + layer.sprite->height);
    }
    const uint32_t dirtyTimer = frame.secondsLeft != drawnSeconds_
        ? verticalBands(scene.timerY, scene.timerY + timerHeight) : 0;
    if (!renderer_->drawCaptureBands(assets_, frame, full ? fullFrame : bands | previousBands_ | dirtyTimer))
        return false;
    drawnSeconds_ = frame.secondsLeft;
    previousBands_ = bands;
    return true;
}

bool CaptureGame::collect(const CaptureAssets::Target &caught)
{
    ++round_.catches;
    if (caught.group != CaptureAssets::noSlot)
    {
        ++round_.groupCatches[caught.group];
        const uint64_t targetBit = 1ULL << caught.slot;
        if ((caughtTargets_ & targetBit) == 0)
        {
            caughtTargets_ |= targetBit;
            ++round_.distinctInGroup[caught.group];
        }
    }
    score_ += caught.points;
    CaptureOutcomeHost::CatchResult result;
    if (!outcomes_->onCaptureOutcome(assets_, &caught, round_, score_, result)) return false;
    for (uint8_t stat = 0; stat <= CaptureAssets::stageDaysSlot; ++stat)
        if ((result.increasedStats & (1U << stat)) != 0) ++round_.statIncreases[stat];
    if (result.firstCatch)
    {
        if (!assets_.readCard(caught.slot, card_)) return false;
        if (card_.pages)
        {
            cardStarted_ = millis();
            cardPage_ = 0;
            state_ = State::Card;
            return renderer_->showCaptureCard(assets_, card_, cardPage_);
        }
    }
    return true;
}

void CaptureGame::readFailed()
{
    assets_.close();
    state_ = State::ReadFailed;
    renderer_->showResourceError("/capture.data");
}

void CaptureGame::redraw()
{
    if (state_ == State::Result) renderer_->showCaptureScore(score_);
    else if (state_ == State::ReadFailed) renderer_->showResourceError("/capture.data");
    else if (state_ == State::Card)
    {
        if (!renderer_->showCaptureCard(assets_, card_, cardPage_)) readFailed();
    }
    else if (state_ != State::Closed && !draw(millis() - started_, true)) readFailed();
}

void CaptureGame::update(bool confirmPending)
{
    if (state_ != State::Swimming && state_ != State::Drop) return;
    const uint32_t age = millis() - started_;
    const auto &scene = assets_.scene();
    bool readable = true;
    if (state_ == State::Drop && age - dropStart_ >= 2U * turnAt_)
    {
        state_ = State::Swimming;
        if (caught_ >= 0)
        {
            Fish &fish = fish_[caught_];
            const CaptureAssets::Target caught = fish.target;
            readable = spawn(fish, age) && collect(caught);
        }
        else
            score_ += scene.missPoints;
    }
    if (!readable)
    {
        readFailed();
        return;
    }
    if (state_ == State::Card) return;
    if (state_ == State::Swimming && !confirmPending && age >= 1000UL * scene.durationSeconds)
    {
        assets_.close();
        CaptureOutcomeHost::CatchResult result;
        if (!outcomes_->onCaptureOutcome(assets_, nullptr, round_, score_, result))
        {
            readFailed();
            return;
        }
        renderer_->showCaptureScore(score_);
        state_ = State::Result;
        return;
    }
    if (age - frameAt_ < frameIntervalMs) return;
    frameAt_ = age;
    if (!draw(age))
    {
        readFailed();
    }
}

void CaptureGame::move(int direction)
{
    if (state_ != State::Swimming) return;
    int x = hookX_ + direction * 8;
    if (x < 0) x = 0;
    if (x >= CaptureAssets::screenWidth) x = CaptureAssets::screenWidth - 1;
    hookX_ = static_cast<uint8_t>(x);
}

void CaptureGame::confirm()
{
    if (state_ == State::Result || state_ == State::ReadFailed)
    {
        cancel();
        return;
    }
    if (state_ == State::Card)
    {
        if (++cardPage_ < card_.pages)
        {
            redraw();
            return;
        }
        if (assets_.scene().pauseReading) started_ += millis() - cardStarted_;
        state_ = State::Swimming;
        redraw();
        return;
    }
    if (state_ != State::Swimming) return;
    const uint32_t age = millis() - started_;
    const auto &scene = assets_.scene();
    if (age >= 1000UL * scene.durationSeconds) return;
    dropStart_ = age;
    turnAt_ = scene.turnMs;
    hookTurnY_ = 2 * scene.depth;
    caught_ = -1;
    state_ = State::Drop;
    traceDrop(age);
}

#endif
