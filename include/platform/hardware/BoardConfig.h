#ifndef BOARD_CONFIG_H
#define BOARD_CONFIG_H

#include <Arduino.h>
#include <Adafruit_ST7735.h>

namespace BoardConfig
{
constexpr int SdCsPin = PB0;

constexpr int TftCsPin = PB12;
constexpr int TftDcPin = PA8;
constexpr int TftRstPin = PB14;
constexpr int TftBacklightPin = PA9;

constexpr int NextCommandButtonPin = PA12;
constexpr int PreviousCommandButtonPin = PA10;
constexpr int ConfirmCommandButtonPin = PA11;

constexpr int buzzerPin = PA4;

constexpr uint32_t SdSpiMhz = 16;
constexpr uint8_t TftInitTab = INITR_BLACKTAB;
constexpr int8_t TftColOffset = 2;
constexpr int8_t TftRowOffset = 1;

constexpr const char *DefaultSpeciesCode = "dino";
constexpr const char *DefaultOutfitCode = "base";
} // namespace BoardConfig

#endif
