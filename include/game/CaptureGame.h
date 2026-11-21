#ifndef CAPTURE_GAME_H
#define CAPTURE_GAME_H

#include "game/CaptureAssets.h"
#include "collection/CollectionGameEvent.h"

class AssetResources;
class Renderer;

class CaptureOutcomeHost
{
public:
    struct CatchResult
    {
        uint16_t increasedStats = 0;
        bool firstCatch = false;
    };

    virtual bool onCaptureOutcome(CaptureAssets &assets, const CaptureAssets::Target *target,
                                  const CollectionRoundStats &round, int32_t score,
                                  CatchResult &result) = 0;
protected:
    ~CaptureOutcomeHost() = default;
};

class CaptureGame
{
public:
    static constexpr int bandRows = 8;
    static constexpr int bandCount = CaptureAssets::screenHeight / bandRows;
    static constexpr int timerHeight = 13;

    struct Layer
    {
        const CaptureAssets::Sprite *sprite;
        int16_t x, y;
        bool flip;
    };

    struct Frame
    {
        Layer layers[CaptureAssets::maxFish + 1];
        uint16_t secondsLeft;
        uint8_t layerCount;
        uint8_t hookX, lineStart, lineEnd;
    };

    bool start(Renderer &renderer, AssetResources &resources, CaptureOutcomeHost &outcomes);
    void cancel();
    void update(bool confirmPending);
    void redraw();
    void move(int direction);
    void confirm();
    bool active() const { return state_ != State::Closed; }
    bool clockRunning() const
    {
        return state_ == State::Swimming || state_ == State::Drop ||
               (state_ == State::Card && !assets_.scene().pauseReading);
    }

private:
    enum class State : uint8_t { Closed, Swimming, Drop, Card, Result, ReadFailed };

    struct Fish
    {
        CaptureAssets::Target target;
        uint32_t born;
        uint16_t offsetX, offsetY;
        bool reverse;
    };

    struct Position
    {
        int x, y; // Half-pixel units, shared by drawing and collision.
        bool movingLeft;
    };

    bool spawn(Fish &fish, uint32_t age);
    bool collect(const CaptureAssets::Target &caught);
    Position position(const Fish &fish, uint32_t age) const;
    void traceDrop(uint32_t age);
    bool draw(uint32_t age, bool full = false);
    void readFailed();
    uint16_t hookY(uint32_t dropAge) const;

    CaptureAssets assets_;
    Renderer *renderer_ = nullptr;
    CaptureOutcomeHost *outcomes_ = nullptr;
    CollectionRoundStats round_ = {};
    CaptureAssets::Card card_ = {};
    uint64_t caughtTargets_ = 0;
    State state_ = State::Closed;
    Fish fish_[CaptureAssets::maxFish] = {};
    uint32_t started_ = 0;
    uint32_t dropStart_ = 0;
    uint32_t frameAt_ = 0;
    uint32_t previousBands_ = 0;
    uint32_t cardStarted_ = 0;
    uint16_t turnAt_ = 0, hookTurnY_ = 0, hitAt_ = 0;
    uint16_t drawnSeconds_ = 0;
    int32_t score_ = 0;
    int8_t caught_ = -1;
    uint8_t hookX_ = 0;
    uint8_t cardPage_ = 0;
};

#endif
