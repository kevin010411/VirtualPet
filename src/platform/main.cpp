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
#include "stm32f1xx_hal.h"

// The stock Arduino main unconditionally calls serialEventRun(), which pulls
// the complete HardwareSerial/Stream/String stack although this firmware has
// no serial event callbacks. Keep the framework's early init ordering and the
// normal setup/loop lifecycle, but omit that unused hook.
__attribute__((constructor(101))) static void firmwarePremain()
{
#ifdef NVIC_PRIORITYGROUP_4
  HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
#endif
#if (__CORTEX_M == 0x07U)
#ifndef I_CACHE_DISABLED
  SCB_EnableICache();
#endif
#ifndef D_CACHE_DISABLED
  SCB_EnableDCache();
#endif
#endif
  init();
}

void setup();
void loop();

int main(void)
{
  initVariant();
  setup();
  for (;;)
  {
#if defined(CORE_CALLBACK)
    CoreCallback();
#endif
    loop();
  }
}

// 建立 TFT 顯示物件
SPIClass SPI_2(PB15, BoardConfig::TftRstPin, PB13);
// TFT reset is sequenced explicitly during startup and recovery. Passing -1
// prevents Adafruit_SPITFT::initSPI() from repeating another 400 ms reset.
CalibratedST7735 tft(&SPI_2, BoardConfig::TftCsPin, BoardConfig::TftDcPin, -1);

SdFat SD;
Pet pet;
PetStorage petStorage(&SD);
Renderer renderer(&tft, &SD);
SdAppearanceLoader appearanceLoader(&SD);
Game game(pet, petStorage, renderer, appearanceLoader);
ButtonInput buttons(
    BoardConfig::PreviousCommandButtonPin,
    BoardConfig::NextCommandButtonPin,
    BoardConfig::ConfirmCommandButtonPin,
    250);

// =========================
// Low battery / PVD 設定
// =========================
static bool g_sdReady = false;
static bool g_gameReady = false;
static bool g_lowBatteryMode = false;
enum class VoltageState : uint8_t
{
  Normal = 0,
  Dimmed = 1,
  Critical = 2
};

static VoltageState g_voltageState = VoltageState::Normal;
static VoltageState g_measuredVoltageState = VoltageState::Normal;
static unsigned long g_lastVoltageSampleAt = 0;
static unsigned long g_voltageRecoverySince = 0;
static unsigned long g_criticalBelowSince = 0;
static const unsigned long kVoltageSampleIntervalMs = 100;
static const unsigned long kVoltageRecoveryConfirmMs = 5000;
static const unsigned long kCriticalStopDelayMs = 5000;
static const unsigned long kPvdSettleUs = 50;
static const uint8_t kPvdLevelDimmed = 7;
static const uint8_t kPvdLevelCritical = 6;

static const uint8_t kBacklightNormal = 255;
static const uint8_t kBacklightDimmed = 128;
static const uint8_t kBacklightCritical = 26;
static const uint8_t kBacklightRampStep = 10;
static const unsigned long kBacklightRampIntervalMs = 20;
static uint8_t g_backlightBrightness = 0;
static uint8_t g_backlightTarget = 0;
static unsigned long g_lastBacklightRampAt = 0;
static bool g_restoreBacklightAfterRender = false;

static const unsigned long kStartupPowerSettleMs = 200;
static const unsigned long kStartupPostSetupMs = 0;
static const unsigned long kTftStartupResetLowMs = 120;
static const unsigned long kTftStartupResetHighMs = 120;

#if ENABLE_DEBUG
struct StartupTiming
{
  unsigned long startedAt = 0;
  unsigned long powerSettleMs = 0;
  unsigned long sdInitMs = 0;
  unsigned long tftInitMs = 0;
  unsigned long gamePrepareMs = 0;
  unsigned long gameRenderMs = 0;
  unsigned long gameSetupMs = 0;
  unsigned long startupReadyMs = 0;
  bool written = false;
};

static StartupTiming g_startupTiming;

static void writeStartupTiming(unsigned long firstFrameMs)
{
  if (g_startupTiming.written)
    return;

  g_startupTiming.written = true;
  File file = SD.open("/boot_timing.txt", O_WRONLY | O_CREAT | O_TRUNC);
  if (!file)
    return;

  char content[192] = {};
  const int length = snprintf(
      content,
      sizeof(content),
      "power_settle_ms=%lu\n"
      "sd_init_ms=%lu\n"
      "tft_init_ms=%lu\n"
      "game_prepare_ms=%lu\n"
      "game_render_ms=%lu\n"
      "game_setup_ms=%lu\n"
      "startup_ready_ms=%lu\n"
      "first_frame_ms=%lu\n",
      g_startupTiming.powerSettleMs,
      g_startupTiming.sdInitMs,
      g_startupTiming.tftInitMs,
      g_startupTiming.gamePrepareMs,
      g_startupTiming.gameRenderMs,
      g_startupTiming.gameSetupMs,
      g_startupTiming.startupReadyMs,
      firstFrameMs);
  if (length > 0 && static_cast<size_t>(length) < sizeof(content))
    file.write(reinterpret_cast<const uint8_t *>(content), static_cast<size_t>(length));
  file.flush();
  file.close();
}
#endif

static uint8_t brightnessForVoltageState(VoltageState state)
{
  switch (state)
  {
  case VoltageState::Critical:
    return kBacklightCritical;
  case VoltageState::Dimmed:
    return kBacklightDimmed;
  case VoltageState::Normal:
  default:
    return kBacklightNormal;
  }
}

static void writeBacklightPwm(uint8_t brightness)
{
  if (brightness == 0)
  {
    analogWrite(BoardConfig::TftBacklightPin, 0);
    pinMode(BoardConfig::TftBacklightPin, OUTPUT);
    digitalWrite(BoardConfig::TftBacklightPin, LOW);
    return;
  }

  analogWrite(BoardConfig::TftBacklightPin, brightness);
}

static void setBacklightImmediate(uint8_t brightness)
{
  g_backlightBrightness = brightness;
  g_backlightTarget = brightness;
  writeBacklightPwm(brightness);
}

static void setBacklightTarget(uint8_t brightness)
{
  g_backlightTarget = brightness;
  if (brightness <= g_backlightBrightness)
  {
    g_backlightBrightness = brightness;
    writeBacklightPwm(brightness);
  }
}

static void updateBacklightRamp(unsigned long now)
{
  if (g_restoreBacklightAfterRender || g_backlightBrightness >= g_backlightTarget)
    return;

  if (now - g_lastBacklightRampAt < kBacklightRampIntervalMs)
    return;

  g_lastBacklightRampAt = now;
  const uint8_t remaining = g_backlightTarget - g_backlightBrightness;
  g_backlightBrightness += remaining > kBacklightRampStep ? kBacklightRampStep : remaining;
  writeBacklightPwm(g_backlightBrightness);
}

static void setPvdThreshold(uint8_t level)
{
  PWR->CR = (PWR->CR & ~(7U << 5)) | ((static_cast<uint32_t>(level) & 7U) << 5);
}

static void initLowBatteryDetector()
{
  // 開啟 PWR 模組時鐘
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;

  // 正常時保持最高 PLS 門檻，量測函式會短暫切到 PLS6。
  setPvdThreshold(kPvdLevelDimmed);

  // 啟用 PVD
  PWR->CR |= PWR_CR_PVDE;
}

static bool isVddBelowPvdThreshold(uint8_t level)
{
  // Restart the comparator so hysteresis from the previous PLS level cannot
  // leak into this measurement when sampling PLS7 and PLS6 back-to-back.
  PWR->CR &= ~PWR_CR_PVDE;
  setPvdThreshold(level);
  PWR->CR |= PWR_CR_PVDE;
  delayMicroseconds(kPvdSettleUs);
  return (PWR->CSR & PWR_CSR_PVDO) != 0; // PVDO = 1 表示 VDD 低於門檻
}

static VoltageState sampleVoltageState()
{
  const bool belowDimmed = isVddBelowPvdThreshold(kPvdLevelDimmed);
  const bool belowCritical = isVddBelowPvdThreshold(kPvdLevelCritical);
  setPvdThreshold(kPvdLevelDimmed);

  if (belowCritical)
    return VoltageState::Critical;
  if (belowDimmed)
    return VoltageState::Dimmed;
  return VoltageState::Normal;
}

static void showSdInitError()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(true);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.print("SD init error");
  setBacklightImmediate(kBacklightNormal);
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

static bool confirmVoltageRecoveredFromCritical()
{
  const unsigned long recoveredSince = millis();
  while (millis() - recoveredSince < kVoltageRecoveryConfirmMs)
  {
    delay(kVoltageSampleIntervalMs);
    if (sampleVoltageState() == VoltageState::Critical)
      return false;
  }

  return true;
}

void enterSleep()
{
  if (g_isSleeping)
    return;

  g_isSleeping = true;
  buttons.clearFlags();
  setBacklightImmediate(0);
  digitalWrite(BoardConfig::buzzerPin, LOW);

  tft.enableDisplay(false);
  tft.enableSleep(true);
  digitalWrite(BoardConfig::TftCsPin, HIGH);
  digitalWrite(BoardConfig::SdCsPin, HIGH);

  while (true)
  {
    // The display is already dark, so PVD is not needed while all clocks stop.
    PWR->CR &= ~PWR_CR_PVDE;
    HAL_SuspendTick();
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    SystemClock_Config();
    HAL_ResumeTick();
    PWR->CR |= PWR_CR_PVDE;

    // Consume the button that woke the MCU. It must not execute a game action.
    buttons.clearFlags();

    const VoltageState measuredState = sampleVoltageState();
    g_measuredVoltageState = measuredState;
    if (measuredState == VoltageState::Critical)
    {
      g_voltageState = VoltageState::Critical;
      g_lowBatteryMode = true;
      g_voltageRecoverySince = 0;
      continue;
    }

    if (g_voltageState == VoltageState::Critical || g_lowBatteryMode)
    {
      // Keep the screen dark while verifying that PLS6 has recovered.
      if (!confirmVoltageRecoveredFromCritical())
      {
        g_voltageState = VoltageState::Critical;
        g_lowBatteryMode = true;
        continue;
      }

      g_voltageState = VoltageState::Dimmed;
      g_measuredVoltageState = sampleVoltageState();
      g_lowBatteryMode = false;
      game.endBatteryAnimation();
      g_criticalBelowSince = 0;
      g_voltageRecoverySince = millis();
    }
    else if (static_cast<uint8_t>(measuredState) > static_cast<uint8_t>(g_voltageState))
    {
      // Voltage fell while sleeping; lowering brightness never needs debounce.
      g_voltageState = measuredState;
    }
    break;
  }

  tft.enableSleep(false);
  delay(120);

  // ST7735 GRAM is not guaranteed to remain intact while sleeping, especially
  // near the low-voltage threshold. Clear every pixel and synchronously redraw
  // the current animation/preview plus all layout slots before display-on and
  // before restoring the backlight.
  tft.fillScreen(ST77XX_BLACK);
  game.redrawAllNow();
  tft.enableDisplay(true);

  noteInteraction(millis());
  g_restoreBacklightAfterRender = true;
  g_isSleeping = false;
}

void wakeFromSleepNow()
{
  // Stop mode resumes inside enterSleep(); this callback is retained only to
  // satisfy ButtonInput's normal-mode interface.
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

static void configureTft()
{
  tft.initR(BoardConfig::TftInitTab);
  tft.setSPISpeed(BoardConfig::TftSpiHz);
  tft.setDisplayOffset(BoardConfig::TftColOffset, BoardConfig::TftRowOffset);
  tft.setRotation(0);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_RED);
}

static void finishPhasedTftStartup()
{
  tft.setDisplayOffset(BoardConfig::TftColOffset, BoardConfig::TftRowOffset);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_RED);
}

static void resetAndInitializeTft(unsigned long lowMs, unsigned long highMs)
{
  TFT_Reset(lowMs, highMs);
  configureTft();
}

static void initializeTftDisplay()
{
  setBacklightImmediate(0);
  resetAndInitializeTft(200, 200);
}

static void beginTftStartupReset()
{
  setBacklightImmediate(0);
  pinMode(BoardConfig::TftRstPin, OUTPUT);
  digitalWrite(BoardConfig::TftRstPin, HIGH);
  delay(10);
  digitalWrite(BoardConfig::TftRstPin, LOW);
}

static void finishTftStartupReset(unsigned long resetLowStartedAt)
{
  const unsigned long elapsedLow = millis() - resetLowStartedAt;
  if (elapsedLow < kTftStartupResetLowMs)
    delay(kTftStartupResetLowMs - elapsedLow);

  digitalWrite(BoardConfig::TftRstPin, HIGH);
  delay(kTftStartupResetHighMs);
}

void onConfirmLongPress()
{
  noteInteraction();
  initializeTftDisplay();
  g_gameReady = game.setup_game();
  setBacklightTarget(brightnessForVoltageState(g_voltageState));
}

static void onLRComboLongPress()
{
  noteInteraction();
  setBacklightImmediate(0);
  g_gameReady = game.resetPet();
  delay(1000);
  setBacklightTarget(brightnessForVoltageState(g_voltageState));
}

static void enterLowBatteryMode(unsigned long now)
{
  if (g_lowBatteryMode)
    return;

  g_lowBatteryMode = true;
  buttons.clearFlags();
  setBacklightTarget(kBacklightCritical);
  game.saveNow();
  game.startBatteryAnimation();
}

static void leaveLowBatteryMode(unsigned long now)
{
  g_lowBatteryMode = false;
  buttons.clearFlags();
  game.endBatteryAnimation();
  noteInteraction(now);
  if (!g_restoreBacklightAfterRender)
    setBacklightTarget(brightnessForVoltageState(g_voltageState));
}

static void applyVoltageState(VoltageState nextState, unsigned long now)
{
  const VoltageState previousState = g_voltageState;
  if (previousState == nextState)
    return;

  g_voltageState = nextState;
  setBacklightTarget(brightnessForVoltageState(nextState));

  if (nextState == VoltageState::Critical)
  {
    enterLowBatteryMode(now);
    return;
  }

  if (previousState == VoltageState::Critical)
    leaveLowBatteryMode(now);
}

static void updateVoltageState(unsigned long now)
{
  if (now - g_lastVoltageSampleAt >= kVoltageSampleIntervalMs)
  {
    g_lastVoltageSampleAt = now;
    g_measuredVoltageState = sampleVoltageState();

    if (g_measuredVoltageState == VoltageState::Critical)
    {
      if (g_criticalBelowSince == 0)
        g_criticalBelowSince = now;
    }
    else
    {
      g_criticalBelowSince = 0;
    }

    const uint8_t measuredRank = static_cast<uint8_t>(g_measuredVoltageState);
    const uint8_t currentRank = static_cast<uint8_t>(g_voltageState);
    if (measuredRank > currentRank)
    {
      // Falling voltage lowers brightness immediately.
      g_voltageRecoverySince = 0;
      applyVoltageState(g_measuredVoltageState, now);
    }
    else if (measuredRank < currentRank)
    {
      if (g_voltageRecoverySince == 0)
      {
        g_voltageRecoverySince = now;
      }
      else if (now - g_voltageRecoverySince >= kVoltageRecoveryConfirmMs)
      {
        // Recover one brightness level at a time so the load rises gradually.
        const VoltageState nextState = static_cast<VoltageState>(currentRank - 1U);
        applyVoltageState(nextState, now);
        g_voltageRecoverySince = now;
      }
    }
    else
    {
      g_voltageRecoverySince = 0;
    }
  }

  if (!g_lowBatteryMode)
    return;

  game.updateBatteryAnimation(now);
  if (g_measuredVoltageState == VoltageState::Critical &&
      g_criticalBelowSince != 0 &&
      now - g_criticalBelowSince >= kCriticalStopDelayMs)
  {
    enterSleep();
  }
}

void setup()
{
#if ENABLE_DEBUG
  g_startupTiming = StartupTiming{};
  g_startupTiming.startedAt = millis();
#endif

#if ENABLE_DEBUG
  const unsigned long powerSettleStartedAt = millis();
#endif
  delay(kStartupPowerSettleMs);
#if ENABLE_DEBUG
  g_startupTiming.powerSettleMs = millis() - powerSettleStartedAt;
#endif

  randomSeed(analogRead(0));

  SPI.begin();   // SPI1
  SPI_2.begin(); // SPI2

  // Keep MCU initialization inside the first TFT reset-low period so the
  // hardware reset remains stable without adding it again to the critical path.
  pinMode(BoardConfig::TftBacklightPin, OUTPUT);
  setBacklightImmediate(0);
  pinMode(BoardConfig::buzzerPin, OUTPUT);
  digitalWrite(BoardConfig::buzzerPin, LOW);
  beginTftStartupReset();
  const unsigned long tftResetLowStartedAt = millis();

  // 初始化 low battery 偵測
  initLowBatteryDetector();

  buttons.begin();

#if ENABLE_DEBUG
  const unsigned long tftInitStartedAt = millis();
#endif
  finishTftStartupReset(tftResetLowStartedAt);
  tft.beginStartup(millis());
  tft.setSPISpeed(BoardConfig::TftSpiHz);

  // The controller must finish software reset before Sleep Out. Start the
  // 500 ms Sleep Out window first, then fill it with SD and game preparation.
  while (!tft.startupWorkWindowOpen())
  {
    tft.advanceStartup(millis());
    delay(1);
  }

#if ENABLE_DEBUG
  const unsigned long sdInitStartedAt = millis();
#endif
  if (!SD.begin(BoardConfig::SdCsPin, SD_SCK_MHZ(BoardConfig::SdSpiMhz)))
  {
#if ENABLE_DEBUG
    g_startupTiming.sdInitMs = millis() - sdInitStartedAt;
#endif
    while (!tft.advanceStartup(millis()))
      delay(1);
    finishPhasedTftStartup();
    showSdInitError();
    return;
  }
#if ENABLE_DEBUG
  g_startupTiming.sdInitMs = millis() - sdInitStartedAt;
#endif

  g_sdReady = true;
#if ENABLE_DEBUG
  const unsigned long gamePrepareStartedAt = millis();
#endif
  const bool gamePrepared = game.prepare_game();
#if ENABLE_DEBUG
  g_startupTiming.gamePrepareMs = millis() - gamePrepareStartedAt;
  g_startupTiming.gameSetupMs = g_startupTiming.gamePrepareMs;
#endif

  while (!tft.advanceStartup(millis()))
    delay(1);
  finishPhasedTftStartup();
#if ENABLE_DEBUG
  g_startupTiming.tftInitMs = millis() - tftInitStartedAt;
  const unsigned long gameFinishStartedAt = millis();
#endif
  g_gameReady = gamePrepared && game.finish_setup_game();
#if ENABLE_DEBUG
  g_startupTiming.gameRenderMs = millis() - gameFinishStartedAt;
  g_startupTiming.gameSetupMs += g_startupTiming.gameRenderMs;
#endif
  if (!g_gameReady)
  {
    if (!gamePrepared)
      game.finish_setup_game();
    return;
  }
  if (kStartupPostSetupMs > 0)
    delay(kStartupPostSetupMs);

  const unsigned long now = millis();
  g_lastVoltageSampleAt = now;
  g_measuredVoltageState = sampleVoltageState();
  if (g_measuredVoltageState != VoltageState::Normal)
    applyVoltageState(g_measuredVoltageState, now);
  else
    setBacklightTarget(kBacklightNormal);

  if (!g_lowBatteryMode)
    game.startStartupAnimation();
  noteInteraction();
#if ENABLE_DEBUG
  g_startupTiming.startupReadyMs = millis() - g_startupTiming.startedAt;
#endif
}

void loop()
{
  const unsigned long now = millis();
  updateBacklightRamp(now);

  if (!g_sdReady)
    return;

  if (!g_gameReady)
    return;

  updateVoltageState(now);
  if (g_lowBatteryMode)
    return;

  buttons.handlePreviousNextComboLongPress(2000, onLRComboLongPress);
  buttons.update(g_isSleeping, onPreviousButton, onNextButton, onConfirmButton, wakeFromSleepNow, playButtonBeep);
  if (g_isSleeping)
    return;

  buttons.handleConfirmLongPress(2000, onConfirmLongPress);

  const unsigned long sleepCheckNow = millis();
  // Idle/base animation remains eligible for sleep.  A queued or active
  // one-shot animation pauses the inactivity clock instead.
  if (game.hasTransientAnimation())
    g_lastInteractionMs = sleepCheckNow;
  if (sleepCheckNow - g_lastInteractionMs >= kSleepTimeoutMs)
  {
    enterSleep();
    return;
  }

  game.loop_game();

#if ENABLE_DEBUG
  if (renderer.hasRenderedFrame() && !g_startupTiming.written)
    writeStartupTiming(millis() - g_startupTiming.startedAt);
#endif

  if (g_restoreBacklightAfterRender)
  {
    g_restoreBacklightAfterRender = false;
    setBacklightTarget(brightnessForVoltageState(g_voltageState));
  }
}
