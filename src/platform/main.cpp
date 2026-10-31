#include "resources/AssetResources.h"
#include "platform/BoardConfig.h"
#include "platform/ButtonInput.h"
#include "platform/CalibratedST7735.h"
#include "platform/RtcClock.h"
#include "platform/SessionClock.h"
#include "platform/PowerController.h"
#include "platform/VoltageMonitor.h"
#include "controller/Game.h"
#include "appearance/SdAppearanceLoader.h"
#include "pet/Pet.h"
#include "display/Renderer.h"
#include "pet/PetStorage.h"
#include "common/FirmwareRandom.h"
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
AssetResources resources(&SD);
Renderer renderer(&tft, resources);
SdAppearanceLoader appearanceLoader(&SD);
Game game(pet, petStorage, renderer, resources, appearanceLoader);
ButtonInput buttons(
    BoardConfig::PreviousCommandButtonPin,
    BoardConfig::NextCommandButtonPin,
    BoardConfig::ConfirmCommandButtonPin);

// =========================
// Low battery / PVD 設定
// =========================
static bool g_sdReady = false;
static bool g_gameReady = false;
static bool g_rtcReady = false;
static SessionClock sessionClock;
static PowerController powerController;
static VoltageMonitor voltageMonitor;
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

static uint8_t desiredBacklight(unsigned long now);

static void showSdInitError()
{
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextWrap(true);
  tft.setTextColor(ST77XX_RED);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.print("SD init error");
  setBacklightTarget(desiredBacklight(millis()));
}

bool g_isSleeping = false;
static bool g_buttonBeepActive = false;
static unsigned long g_buttonBeepStartedAt = 0;

static void updateButtonBeep(unsigned long now)
{
#if ENABLE_SEQUENCE_AUDIO
  if (game.audioActive()) { g_buttonBeepActive = false; return; }
#endif
  if (g_buttonBeepActive && now - g_buttonBeepStartedAt >= 35)
  {
    digitalWrite(BoardConfig::buzzerPin, LOW);
    g_buttonBeepActive = false;
  }
}

void playButtonBeep()
{
#if ENABLE_SEQUENCE_AUDIO
  if (game.audioActive()) return;
#endif
  digitalWrite(BoardConfig::buzzerPin, HIGH);
  g_buttonBeepStartedAt = millis();
  g_buttonBeepActive = true;
}

static uint8_t desiredBacklight(unsigned long now)
{
  return powerController.brightness(now, brightnessForVoltageState(voltageMonitor.state()));
}

void playCheatStageDaysBeep()
{
  playButtonBeep();
  delay(35);
  updateButtonBeep(millis());
  delay(65);
  playButtonBeep();
}

void noteInteraction(unsigned long now = millis())
{
  powerController.interact(now);
}

static void synchronizeGameTime()
{
  uint32_t rtcSeconds = 0, epoch = 0;
  g_rtcReady = RtcClock::pollSeconds(rtcSeconds, epoch);
  const uint32_t seconds = sessionClock.update(millis(), g_rtcReady, rtcSeconds, epoch);
  if (!voltageMonitor.isCritical()) game.synchronizeTime(seconds);
}

static bool confirmVoltageRecoveredFromCritical()
{
  const unsigned long recoveredSince = millis();
  while (millis() - recoveredSince < VoltageMonitor::kRecoveryConfirmMs)
  {
    delay(VoltageMonitor::kSampleIntervalMs);
    if (sampleVoltageState() == VoltageState::Critical)
      return false;
  }

  return true;
}

void enterSleep()
{
  const bool deadBeforeSleep = game.isDead();
  uint32_t lastWake = buttons.wakeGeneration();
  if (g_isSleeping || !g_rtcReady)
    return;

  game.stopPlaybackForSleep();
  g_isSleeping = true;
  buttons.clearFlags();
  setBacklightImmediate(0);
  digitalWrite(BoardConfig::buzzerPin, LOW);
  g_buttonBeepActive = false;

  tft.enableDisplay(false);
  tft.enableSleep(true);
  delay(120);
  digitalWrite(PA3, HIGH); // Active-low board indicator off.
  digitalWrite(BoardConfig::TftCsPin, HIGH);
  digitalWrite(BoardConfig::SdCsPin, HIGH);

  while (true)
  {
    // The display is already dark, so PVD is not needed while all clocks stop.
    PWR->CR &= ~PWR_CR_PVDE;
    HAL_SuspendTick();
    // A pending EXTI wakes WFI even with PRIMASK set. Do not clear a late edge.
    __disable_irq();
    if (!buttons.isHeld() && buttons.wakeGeneration() == lastWake)
      HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);
    __enable_irq();
    SystemClock_Config();
    HAL_ResumeTick();
    PWR->CR |= PWR_CR_PVDE;
    const uint32_t currentWake = buttons.wakeGeneration();
    const bool buttonWake = currentWake != lastWake || buttons.isHeld();
    lastWake = currentWake;
    if (!buttonWake) continue; // Pending non-button IRQs must not reveal the screen.

    // Consume the button that woke the MCU. It must not execute a game action.
    buttons.clearFlags();

    const VoltageMonitor::WakeResult wake = voltageMonitor.observeWake(sampleVoltageState());
    if (wake == VoltageMonitor::WakeResult::Retry)
    {
      if (buttons.isHeld()) delay(VoltageMonitor::kSampleIntervalMs);
      continue;
    }

    if (wake == VoltageMonitor::WakeResult::ConfirmRecovery)
    {
      // Keep the screen dark while verifying that PLS6 has recovered.
      if (!confirmVoltageRecoveredFromCritical())
        continue;

      const VoltageState recoveredSample = sampleVoltageState();
      game.endBatteryAnimation();
      voltageMonitor.completeWakeRecovery(recoveredSample, millis());
    }
    break;
  }

  tft.enableSleep(false);
  delay(60);
  RtcClock::requestSynchronization();
  if (g_gameReady)
    synchronizeGameTime();
  if (deadBeforeSleep && !voltageMonitor.isCritical() && game.isDead() && game.isReady())
  {
    game.OnDeathKey();
    buttons.clearFlags();
  }

  // ST7735 GRAM is not guaranteed to remain intact while sleeping, especially
  // near the low-voltage threshold. Clear every pixel and synchronously redraw
  // the current animation/preview plus all layout slots before display-on and
  // before restoring the backlight.
  tft.fillScreen(ST77XX_BLACK);
  if (!g_sdReady)
    showSdInitError();
  else if (!g_gameReady)
    game.finish_setup_game(); // Repaint the original Session/startup diagnostic.
  else if (!game.isReady())
    renderer.showResourceError();
  else
    game.redrawAllNow();
  setBacklightImmediate(0);
  tft.enableDisplay(true);

  noteInteraction(millis());
  g_restoreBacklightAfterRender = true;
  g_isSleeping = false;
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

static void restartTftFromCheatMode()
{
  noteInteraction();
  initializeTftDisplay();
  game.redrawAllNow();
  synchronizeGameTime();
  setBacklightTarget(desiredBacklight(millis()));
}

static void resetPetFromCheatMode()
{
  noteInteraction();
  setBacklightImmediate(0);
  g_gameReady = game.resetPet();
  delay(1000);
  synchronizeGameTime();
  setBacklightTarget(desiredBacklight(millis()));
}

static void enterLowBatteryMode()
{
  buttons.clearFlags();
  setBacklightTarget(kBacklightCritical);
  if (g_gameReady && game.isReady())
  {
    game.saveNow();
    game.startBatteryAnimation();
  }
}

static void leaveLowBatteryMode(unsigned long now)
{
  buttons.clearFlags();
  game.endBatteryAnimation();
  noteInteraction(now);
  if (!g_restoreBacklightAfterRender)
    setBacklightTarget(desiredBacklight(now));
}

static void applyVoltageTransition(VoltageState previousState, unsigned long now)
{
  const VoltageState nextState = voltageMonitor.state();
  if (previousState == nextState)
    return;

  if (!g_restoreBacklightAfterRender)
    setBacklightTarget(desiredBacklight(now));

  if (nextState == VoltageState::Critical)
  {
    enterLowBatteryMode();
    return;
  }

  if (previousState == VoltageState::Critical)
    leaveLowBatteryMode(now);
}

static void updateVoltageState(unsigned long now)
{
  if (voltageMonitor.sampleDue(now))
  {
    const VoltageState previousState = voltageMonitor.state();
    voltageMonitor.observe(now, sampleVoltageState());
    applyVoltageTransition(previousState, now);
  }

  if (!voltageMonitor.isCritical())
    return;

  game.updateBatteryAnimation(now);
  if (voltageMonitor.shouldStop(now))
    enterSleep();
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

  FirmwareRandom::seed(analogRead(0));

  SPI.begin();   // SPI1
  SPI_2.begin(); // SPI2

  // Keep MCU initialization inside the first TFT reset-low period so the
  // hardware reset remains stable without adding it again to the critical path.
  pinMode(BoardConfig::TftBacklightPin, OUTPUT);
  setBacklightImmediate(0);
  pinMode(BoardConfig::buzzerPin, OUTPUT);
  digitalWrite(BoardConfig::buzzerPin, LOW);
  beginTftStartupReset();
  pinMode(PA3, OUTPUT);
  digitalWrite(PA3, HIGH);
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
  // Startup may wait for LSE once. A failed clock must not prevent SD/session
  // startup; subsequent recovery is polled without blocking input.
  RtcClock::begin();
  synchronizeGameTime();
  if (!SD.begin(BoardConfig::SdCsPin, SD_SCK_MHZ(BoardConfig::SdSpiMhz)))
  {
#if ENABLE_DEBUG
    g_startupTiming.sdInitMs = millis() - sdInitStartedAt;
#endif
    while (!tft.advanceStartup(millis()))
      delay(1);
    finishPhasedTftStartup();
    showSdInitError();
    noteInteraction();
    return;
  }
#if ENABLE_DEBUG
  g_startupTiming.sdInitMs = millis() - sdInitStartedAt;
#endif

  g_sdReady = true;
  // Keep the temporary boot-stage labels visible before any potentially
  // blocking SD read begins.  Normally game preparation overlaps this window.
  while (!tft.advanceStartup(millis()))
    delay(1);
  finishPhasedTftStartup();
  setBacklightImmediate(kBacklightNormal);
#if ENABLE_DEBUG
  const unsigned long gamePrepareStartedAt = millis();
#endif
  const bool gamePrepared = game.prepare_game();
#if ENABLE_DEBUG
  g_startupTiming.gamePrepareMs = millis() - gamePrepareStartedAt;
  g_startupTiming.gameSetupMs = g_startupTiming.gamePrepareMs;
#endif

#if ENABLE_DEBUG
  g_startupTiming.tftInitMs = millis() - tftInitStartedAt;
  const unsigned long gameFinishStartedAt = millis();
#endif
  // Capture the Session baseline after SD preparation, immediately before ready.
  synchronizeGameTime();
  g_gameReady = gamePrepared && game.finish_setup_game();
#if ENABLE_DEBUG
  g_startupTiming.gameRenderMs = millis() - gameFinishStartedAt;
  g_startupTiming.gameSetupMs += g_startupTiming.gameRenderMs;
#endif
  if (!g_gameReady)
  {
    if (!gamePrepared)
      game.finish_setup_game();
    // finish_setup_game() renders a concise resource error for a failed
    // runtime load, but the normal backlight target is only applied after a
    // successful boot.  Reveal that error instead of leaving a black screen.
    setBacklightImmediate(kBacklightNormal);
    noteInteraction();
    return;
  }
  if (kStartupPostSetupMs > 0)
    delay(kStartupPostSetupMs);

  const unsigned long now = millis();
  const VoltageState previousState = voltageMonitor.state();
  voltageMonitor.begin(now, sampleVoltageState());
  if (voltageMonitor.state() != VoltageState::Normal)
    applyVoltageTransition(previousState, now);
  else
    setBacklightTarget(kBacklightNormal);

  if (!voltageMonitor.isCritical())
    game.startStartupAnimation();
  buttons.clearFlags();
  noteInteraction();
#if ENABLE_DEBUG
  g_startupTiming.startupReadyMs = millis() - g_startupTiming.startedAt;
#endif
}

void loop()
{
  const unsigned long now = millis();
  updateButtonBeep(now);
  updateVoltageState(now);
  const bool ready = g_sdReady && g_gameReady && game.isReady();
  if (voltageMonitor.isCritical())
  {
    if (ready) synchronizeGameTime();
    return;
  }
  CheatButtonEvent buttonEvent = CheatButtonEvent::None;
  const bool deadBeforeTime = game.isDead();
  if (ready) synchronizeGameTime();
  if (!deadBeforeTime && game.isDead()) buttons.clearFlags();
  if (ready && g_gameReady && game.isReady())
  {
    if (game.isDead())
    {
      if (buttons.takeDeathPress())
      {
        noteInteraction();
        playButtonBeep();
        game.OnDeathKey();
        buttons.clearFlags();
      }
    }
    else
    {
#if ENABLE_INSTRUCTION_RESPONSE
      static bool captured = false;
      if (game.capturesInstructionInput())
      {
        captured = true;
        uint8_t held = 0, pressed = 0; uint32_t timestamp = 0;
        for (uint8_t edge = 0; edge < 3; ++edge)
        {
          buttons.instructionSample(held, pressed, timestamp);
          game.instructionInput(held, pressed, timestamp);
        }
        game.instructionPending(buttons.instructionPending(), buttons.instructionPendingAt(), buttons.instructionRawMask());
        if (held) noteInteraction();
      }
      else
      {
        if (captured) { buttons.clearFlags(); captured = false; }
#endif
        buttonEvent = buttons.update(onPreviousButton, onNextButton, onConfirmButton, playButtonBeep);
#if ENABLE_INSTRUCTION_RESPONSE
      }
#endif
    }
  }
  else
    buttons.clearFlags(); // Fatal screens discard gestures.
  if (buttons.isDebouncedHeld()) noteInteraction();
  switch (buttonEvent)
  {
  case CheatButtonEvent::Entered:
    noteInteraction();
    playButtonBeep();
    break;
  case CheatButtonEvent::RestartTft:
    restartTftFromCheatMode();
    break;
  case CheatButtonEvent::ResetPet:
    resetPetFromCheatMode();
    break;
  case CheatButtonEvent::SetStageDays:
    noteInteraction();
    if (game.setStageDaysForCheat(Pet::kStageDaysMax))
      playCheatStageDaysBeep();
    break;
  case CheatButtonEvent::Exited:
  case CheatButtonEvent::None:
    break;
  }

  // Complete elapsed progress before input; this second observation includes
  // time consumed by input handling without giving expired care a late rescue.
  if (g_sdReady && g_gameReady && game.isReady()) synchronizeGameTime();

  const unsigned long sleepCheckNow = millis();
  const bool busy = !g_rtcReady || buttons.isHeld() ||
      (g_sdReady && SD.card()->isBusy()) ||
      (g_gameReady && game.isReady() && (game.preventsIdleSleep() || game.hasPendingTime()));
  if (powerController.shouldSleep(sleepCheckNow, busy))
  {
    enterSleep();
    return;
  }

  const bool deadBeforeLoop = game.isDead();
  if (g_gameReady && game.isReady()) game.loop_game();
  if (!deadBeforeLoop && game.isDead()) buttons.clearFlags();
  updateButtonBeep(millis());

#if ENABLE_DEBUG
  if (renderer.hasRenderedFrame() && !g_startupTiming.written)
    writeStartupTiming(millis() - g_startupTiming.startedAt);
#endif

  if (g_restoreBacklightAfterRender &&
      (!g_gameReady || !game.isReady() || !game.hasPendingTime() || game.hasTransientAnimation()))
  {
    g_restoreBacklightAfterRender = false;
  }
  setBacklightTarget(g_restoreBacklightAfterRender ? 0 : desiredBacklight(millis()));
  updateBacklightRamp(millis());
}
