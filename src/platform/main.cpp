#include "platform/hardware/BoardConfig.h"
#include "platform/hardware/ButtonInput.h"
#include "platform/hardware/CalibratedST7735.h"
#include "presentation/application/Game.h"
#include "appearance/adapters/SdAppearanceLoader.h"
#include "pet/domain/Pet.h"
#include "presentation/adapters/rendering/Renderer.h"
#include "pet/adapters/PetStorage.h"
#include <SdFat.h>
#include "stm32f1xx.h"

// 建立 TFT 顯示物件
SPIClass SPI_2(PB15, BoardConfig::TftRstPin, PB13);
CalibratedST7735 tft(&SPI_2, BoardConfig::TftCsPin, BoardConfig::TftDcPin, BoardConfig::TftRstPin);

SdFat SD;
Pet pet;
PetStorage petStorage(&SD);
Renderer *renderer = Renderer::create(&tft, &SD);
SdAppearanceLoader appearanceLoader(&SD);
Game game(pet, petStorage, *renderer, appearanceLoader);
ButtonInput buttons(
    BoardConfig::PreviousCommandButtonPin,
    BoardConfig::NextCommandButtonPin,
    BoardConfig::ConfirmCommandButtonPin,
    250);

// =========================
// Low battery / PVD 設定
// =========================
static bool g_sdReady = false;
static bool g_lowBatteryMode = false;
static unsigned long g_lowBatterySince = 0;
static unsigned long g_lowBatteryEnteredAt = 0;
static const unsigned long kLowBatteryConfirmMs = 800; // 防抖動誤判
static const unsigned long kLowBatteryAnimationMs = 5000;
static unsigned long g_lowBatteryRecoveredSince = 0;
static const unsigned long kLowBatteryRecoverConfirmMs = 500;
static const bool kForceLowBatteryTest = false; // 測試 low battery 時改成 true
static const unsigned long kStartupPowerSettleMs = 300;
static const unsigned long kStartupPostSetupMs = 0;
static const unsigned long kTftPowerSettleMs = 150;
static const unsigned long kTftRetryGapMs = 60;
static const unsigned long kTftStartupResetLowMs = 120;
static const unsigned long kTftStartupResetHighMs = 120;

static void initLowBatteryDetector()
{
  // 開啟 PWR 模組時鐘
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;

  // 清掉 PLS[7:5]
  PWR->CR &= ~(7U << 5);

  // 設最高門檻（較早警告）
  // 若之後覺得太早，可改成 6U 或 5U
  PWR->CR |= (8U << 5);

  // 啟用 PVD
  PWR->CR |= PWR_CR_PVDE;
}

static bool isVddBelowPvdThreshold()
{
  if (kForceLowBatteryTest)
    return true;

  // PVDO = 1 表示 VDD 低於設定門檻
  return (PWR->CSR & PWR_CSR_PVDO) != 0;
}

static void showSdInitError()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(true);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.print("SD init error");
  digitalWrite(BoardConfig::TftBacklightPin, HIGH);
}

const unsigned long kSleepTimeoutMs = 1UL * 60UL * 1000UL; // 1分鐘會自動休眠
unsigned long g_lastInteractionMs = 0;
bool g_isSleeping = false;

void playButtonBeep()
{
  digitalWrite(BoardConfig::buzzerPin, HIGH);
  delay(35);
  digitalWrite(BoardConfig::buzzerPin, LOW);
}

void noteInteraction(unsigned long now = millis())
{
  g_lastInteractionMs = now;
}

void enterSleep()
{
  g_isSleeping = true;
  digitalWrite(BoardConfig::TftBacklightPin, LOW);
}

void wakeFromSleep(unsigned long now)
{
  buttons.clearFlags();
  digitalWrite(BoardConfig::TftBacklightPin, HIGH);
  game.requestFullRedraw();
  noteInteraction(now);
  g_isSleeping = false;
}

void wakeFromSleepNow()
{
  wakeFromSleep(millis());
}

void onPreviousButton()
{
  game.OnRightKey();
  noteInteraction();
}

void onNextButton()
{
  game.OnLeftKey();
  noteInteraction();
}

void onConfirmButton()
{
  game.OnConfirmKey();
  noteInteraction();
}

static void TFT_Reset(unsigned long lowMs, unsigned long highMs)
{
  pinMode(BoardConfig::TftRstPin, OUTPUT);
  digitalWrite(BoardConfig::TftRstPin, HIGH);
  delay(10);
  digitalWrite(BoardConfig::TftRstPin, LOW);
  delay(lowMs);
  digitalWrite(BoardConfig::TftRstPin, HIGH);
  delay(highMs);
}

static void TFT_Reset200ms()
{
  TFT_Reset(200, 200);
}

static void initializeTftDisplay(bool retryAfterPowerOn)
{
  digitalWrite(BoardConfig::TftBacklightPin, LOW);

  if (retryAfterPowerOn)
    delay(kTftPowerSettleMs);

  const uint8_t initAttempts = retryAfterPowerOn ? 2 : 1;
  for (uint8_t attempt = 0; attempt < initAttempts; ++attempt)
  {
    if (retryAfterPowerOn)
      TFT_Reset(kTftStartupResetLowMs, kTftStartupResetHighMs);
    else
      TFT_Reset200ms();

    tft.initR(BoardConfig::TftInitTab);
    tft.setDisplayOffset(BoardConfig::TftColOffset, BoardConfig::TftRowOffset);
    tft.setRotation(2);
    tft.fillScreen(ST77XX_BLACK);

    if (attempt + 1 < initAttempts)
      delay(kTftRetryGapMs);
  }

  tft.setTextColor(ST77XX_RED);
}

void onConfirmLongPress()
{
  noteInteraction();
  initializeTftDisplay(false);
  digitalWrite(BoardConfig::TftBacklightPin, HIGH);
  game.setup_game();
}

static void onLRComboLongPress()
{
  noteInteraction();
  digitalWrite(BoardConfig::TftBacklightPin, LOW);
  game.resetPet();
  delay(1000);
  digitalWrite(BoardConfig::TftBacklightPin, HIGH);
}

static void enterLowBatteryMode(unsigned long now)
{
  if (g_lowBatteryMode)
    return;

  g_lowBatteryMode = true;
  g_lowBatteryEnteredAt = now;
  buttons.clearFlags();
  g_isSleeping = false;
  digitalWrite(BoardConfig::TftBacklightPin, HIGH);
  game.saveNow();
  game.startBatteryAnimation();
}

static void leaveLowBatteryMode(unsigned long now)
{
  g_lowBatteryMode = false;
  g_lowBatteryEnteredAt = 0;
  g_lowBatteryRecoveredSince = 0;
  buttons.clearFlags();
  if (g_isSleeping)
  {
    wakeFromSleep(now);
    return;
  }

  digitalWrite(BoardConfig::TftBacklightPin, HIGH);
  game.requestFullRedraw();
  noteInteraction(now);
}

static void updateLowBatteryMode(unsigned long now)
{
  if (isVddBelowPvdThreshold())
  {
    g_lowBatteryRecoveredSince = 0;
    if (g_lowBatterySince == 0)
      g_lowBatterySince = now;

    if (!g_lowBatteryMode && now - g_lowBatterySince >= kLowBatteryConfirmMs)
      enterLowBatteryMode(now);

    if (g_lowBatteryMode && !g_isSleeping)
    {
      game.updateBatteryAnimation(now);
      if (now - g_lowBatteryEnteredAt >= kLowBatteryAnimationMs)
        enterSleep();
    }
    return;
  }

  g_lowBatterySince = 0;
  if (!g_lowBatteryMode)
    return;

  if (g_lowBatteryRecoveredSince == 0)
    g_lowBatteryRecoveredSince = now;

  if (now - g_lowBatteryRecoveredSince >= kLowBatteryRecoverConfirmMs)
    leaveLowBatteryMode(now);
}

void setup()
{
  delay(kStartupPowerSettleMs);

  randomSeed(analogRead(0));

  SPI.begin();   // SPI1
  SPI_2.begin(); // SPI2

  // 初始化 TFT 螢幕
  pinMode(BoardConfig::TftBacklightPin, OUTPUT);
  digitalWrite(BoardConfig::TftBacklightPin, LOW);
  pinMode(BoardConfig::buzzerPin, OUTPUT);
  initializeTftDisplay(true);

  // 初始化 low battery 偵測
  initLowBatteryDetector();

  buttons.begin();

  if (!SD.begin(BoardConfig::SdCsPin, SD_SCK_MHZ(BoardConfig::SdSpiMhz)))
  {
    showSdInitError();
    SD.initErrorPrint(&tft);
    return;
  }

  g_sdReady = true;
  game.setRendererAssetAppearance(BoardConfig::DefaultSpeciesCode, BoardConfig::DefaultOutfitCode);
  game.setup_game();
  if (kStartupPostSetupMs > 0)
    delay(kStartupPostSetupMs);
  digitalWrite(BoardConfig::TftBacklightPin, HIGH);
  game.startStartupAnimation();
  noteInteraction();
}

void loop()
{
  // 每圈都把 TFT_RST(PB14) 維持在推挽輸出 HIGH
  pinMode(BoardConfig::TftRstPin, OUTPUT);
  digitalWrite(BoardConfig::TftRstPin, HIGH);

  const unsigned long now = millis();

  if (!g_sdReady)
    return;

  updateLowBatteryMode(now);
  if (g_lowBatteryMode)
    return;

  buttons.handlePreviousNextComboLongPress(2000, onLRComboLongPress);
  buttons.update(g_isSleeping, onPreviousButton, onNextButton, onConfirmButton, wakeFromSleepNow, playButtonBeep);
  if (g_isSleeping)
    return;

  buttons.handleConfirmLongPress(2000, onConfirmLongPress);

  const unsigned long sleepCheckNow = millis();
  if (sleepCheckNow - g_lastInteractionMs >= kSleepTimeoutMs)
  {
    enterSleep();
    return;
  }

  game.loop_game();
}
