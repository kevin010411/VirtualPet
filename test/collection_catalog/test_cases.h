#pragma once

#include <filesystem>
#include "common/AppProfile.h"

class Game;
class SdFat;

extern unsigned long hostMillis;
void tick(Game &game, unsigned long now);
void mountBundle(SdFat &sd, const std::filesystem::path &root);

#if ENABLE_CAPTURE_GAME
void verifyCaptureFishLifetime();
void verifyPreCaptureSave();
void verifyCapture(const std::filesystem::path &root, bool shouldPause);
#endif

#if ENABLE_SHOP
int verifyShop(const std::filesystem::path &root);
#endif

#if ENABLE_STAT_EVENTS
void verifyBingbingEvents(const std::filesystem::path &root);
#endif
