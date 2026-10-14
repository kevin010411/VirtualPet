#ifndef ANIMATION_CONTROLLER_H
#define ANIMATION_CONTROLLER_H

#include <Arduino.h>
#include <SdFat.h>
#include "animation/BaseAnimationRotation.h"
#include "animation/Animation.h"

class Renderer;
struct PetBehaviorConfig;

class AnimationController
{
public:
    explicit AnimationController(Renderer &renderer);

    void configureRuntimeContract(const PetBehaviorConfig &config);
    void setup(const AssetData::AnimationRef &baseAnimation);
    void setBaseAnimation(const AssetData::AnimationRef &baseAnimation,
                          uint8_t stateSlot = UINT8_MAX,
                          const SequenceAudioRef &audio = {});
    bool hasAnimation(FirmwarePlaybackRole id) const;
    bool hasAnimation(const AssetData::AnimationRef &animation) const;
    bool hasActionAnimation(FirmwarePlaybackRole id) const;
    bool hasAnimations(const FirmwarePlaybackRole *ids, size_t count) const;
    PlaybackResult replace(const AnimationSequence &sequence);
    void cancelAll();
    bool isBusy() const;
    bool hasFinitePlayback() const;
#if ENABLE_SEQUENCE_AUDIO
    bool audioActive() const { return audioPlayer.audible(); }
    SequenceAudioError audioError() const { return audioPlayer.firstError(); }
#endif
    bool hasAnimationPending(FirmwarePlaybackRole id) const;
    FirmwarePlaybackRole currentPlaybackRole() const;
    uint8_t currentVersionIndex() const;
    void requestFullRedraw();
    bool takeFullRedrawRequest();
    void preparePlayback(unsigned long now);
    PlaybackTickResult tick(unsigned long now);
    void startBatteryAnimation();
    void updateBatteryAnimation(unsigned long now);
    unsigned long frameIntervalFor(FirmwarePlaybackRole id) const;
    uint16_t frameCountFor(FirmwarePlaybackRole id) const;
    uint16_t frameCountFor(const AssetData::AnimationRef &animation) const;
    uint16_t versionCountFor(const AssetData::AnimationRef &animation) const;
    SdFat *sdCard() const;

private:
    static constexpr unsigned long frameIntervalSlow = 600;
    static constexpr uint8_t kMaxQueuedAnimations = 8;

    Renderer &renderer;
    Animation animationQueue[kMaxQueuedAnimations] = {};
    uint8_t animationQueueCount = 0;
    Animation activeAnimation = {};
    const PetBehaviorConfig *runtimeContract = nullptr;
    bool hasActiveAnimation = false;
    uint8_t activeRepeatsRemaining = 0;
    BaseAnimationRotation baseRotation;
    long displayDuration = 0;
    bool dirtyAnimation = true;
    bool animateDone = true;
    unsigned long frameInterval = frameIntervalSlow;
    unsigned long lastFrameTime = 0;
    unsigned long lastPlaybackUpdateTime = 0;
    FirmwarePlaybackRole showPlaybackRole = FirmwarePlaybackRole::None;
    AssetData::AnimationRef showAnimation = {};
    uint8_t showVersionIndex = 0;
    bool playbackFailedThisTick = false;
    FirmwarePlaybackRole playbackFailedRoleThisTick = FirmwarePlaybackRole::None;
    bool playbackPrepared = false;
    bool preparedTargetChanged = false;
    bool preparedFrameDue = false;
    bool fullRedrawRequested = false;

#if ENABLE_SEQUENCE_AUDIO
    SequenceAudioPlayer audioPlayer;
    SequenceAudioRef baseAudio;
    bool synchronizedBase = false;
    bool baseStarted = false;
    uint32_t baseLoopDuration = 0;
    uint16_t baseFrame = 0;
    void prepareBasePlayback(unsigned long now);
    PlaybackTickResult tickBasePlayback();
    bool synchronizedSequence = false;
    bool sequenceStarted = false;
    bool sequenceLoops = false;
    uint32_t synchronizedPassMs[kMaxQueuedAnimations] = {};
    uint32_t synchronizedDurationMs[kMaxQueuedAnimations] = {};
    uint32_t synchronizedSegmentElapsedMs = 0;
    uint16_t synchronizedIntervalMs[kMaxQueuedAnimations] = {};
    uint8_t synchronizedSegment = 255;
    uint8_t synchronizedRepeat = 255;
    uint16_t synchronizedFrame = 0;
    bool synchronizedTargetPending = false;
    SequenceAudioRef audioFor(const AssetData::AnimationRef &animation) const;
    void prepareSynchronizedPlayback(unsigned long now);
    PlaybackTickResult tickSynchronizedPlayback();
#endif
    void resetPlaybackState();
    unsigned long completePlaybackDuration(uint16_t frameCount, unsigned long frameIntervalMs) const;
    void updateElapsed(unsigned long elapsed);
    void completeActiveAnimation();
    void renderPrepared();
    void tryStartNextAnimation();
    unsigned long resolvedDuration(const Animation &animation) const;
    PlaybackResult validate(const Animation &animation) const;
    AssetData::AnimationRef systemAnimation(FirmwarePlaybackRole id) const;
    AssetData::AnimationRef resolvedAnimation(const Animation &animation) const;
};

#endif // ANIMATION_CONTROLLER_H
