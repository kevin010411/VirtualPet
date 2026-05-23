#include <Adafruit_ST7735.h>
#include "Game.h"
#include <SdFat.h>
#include "stm32f1xx.h"

const int SD_CS = PB0; // ← 經 74VHC125 緩衝到 CARD_CS

// TFT (SPI2)
const int TFT_CS = PB12;
const int TFT_DC = PA8;
const int TFT_RST = PB14;
const int TFT_BLK = PA9;

const int NEXT_COMMAND_BUTTON = PA12;
const int PREVIOUS_COMMAND_BUTTON = PA10;
const int CONFIRM_COMMAND_BUTTON = PA11;

// 建議頻率
const uint32_t SD_SPI_MHZ = 16;
const uint8_t TFT_INIT_TAB = INITR_BLACKTAB;
constexpr Renderer::AssetFormatPreference kAssetFormatPreference = Renderer::AssetFormatPreference::PreferRle;
constexpr const char *kAnimalAssetName = "dino";

// 建立 TFT 顯示物件
SPIClass SPI_2(PB15, TFT_RST, PB13);
Adafruit_ST7735 tft(&SPI_2, TFT_CS, TFT_DC, TFT_RST);

SdFat SD;
Game game(&tft, &SD);

// =========================
// Low battery / PVD 設定
// =========================
static bool g_sdReady = false;
static bool g_lowBatteryMode = false;
static unsigned long g_lowBatterySince = 0;
static unsigned long g_lowBatteryEnteredAt = 0;
static const unsigned long LOW_BAT_CONFIRM_MS = 8000; // 防抖動誤判
static const unsigned long LOW_BAT_ANIMATION_MS = 5000;
static unsigned long g_lowBatteryRecoveredSince = 0;
static const unsigned long LOW_BAT_RECOVER_CONFIRM_MS = 500;
static const bool FORCE_LOW_BATTERY_TEST = true; // 測試 low battery 時改成 true

static void initLowBatteryDetector()
{
  // 開啟 PWR 模組時鐘
  RCC->APB1ENR |= RCC_APB1ENR_PWREN;

  // 清掉 PLS[7:5]
  PWR->CR &= ~(7U << 5);

  // 設最高門檻（較早警告）
  // 若之後覺得太早，可改成 6U 或 5U
  PWR->CR |= (7U << 5);

  // 啟用 PVD
  PWR->CR |= PWR_CR_PVDE;
}

static bool isVddBelowPvdThreshold()
{
  if (FORCE_LOW_BATTERY_TEST)
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
  digitalWrite(TFT_BLK, HIGH);
}

const unsigned long BUTTON_COOLDOWN = 50;                    // 0.05 秒
const unsigned long SLEEP_TIMEOUT_MS = 10UL * 60UL * 1000UL; // 10分鐘會自動休眠
unsigned long lastPrevPressTime = 0;
unsigned long lastNextPressTime = 0;
unsigned long lastConfirmPressTime = 0;
unsigned long lastInteractionMs = 0;
volatile bool PreviousButtonPressed = false;
volatile bool ConfirmButtonPressed = false;
volatile bool NextButtonPressed = false;
bool isSleeping = false;

void handlePreviousButtonInterrupt()
{
  if (digitalRead(PREVIOUS_COMMAND_BUTTON) == HIGH)
    return;
  PreviousButtonPressed = true;
}

void handleConfirmButtonInterrupt()
{
  if (digitalRead(CONFIRM_COMMAND_BUTTON) == HIGH)
    return;
  ConfirmButtonPressed = true;
}

void handleNextButtonInterrupt()
{
  if (digitalRead(NEXT_COMMAND_BUTTON) == HIGH)
    return;
  NextButtonPressed = true;
}

bool hasPendingButtonPress()
{
  return PreviousButtonPressed || ConfirmButtonPressed || NextButtonPressed;
}

bool isLRComboHeld()
{
  return digitalRead(PREVIOUS_COMMAND_BUTTON) == LOW &&
         digitalRead(NEXT_COMMAND_BUTTON) == LOW;
}

void clearButtonFlags()
{
  PreviousButtonPressed = false;
  ConfirmButtonPressed = false;
  NextButtonPressed = false;
}

void noteInteraction(unsigned long now = millis())
{
  lastInteractionMs = now;
}

void enterSleep()
{
  isSleeping = true;
  digitalWrite(TFT_BLK, LOW);
}

void wakeFromSleep(unsigned long now)
{
  clearButtonFlags();
  digitalWrite(TFT_BLK, HIGH);
  game.requestFullRedraw();
  noteInteraction(now);
  isSleeping = false;
}

void buttonDetect()
{
  unsigned long now = millis();

  if (isLRComboHeld())
  {
    PreviousButtonPressed = false;
    NextButtonPressed = false;
    return;
  }

  if (isSleeping)
  {
    if (hasPendingButtonPress())
      wakeFromSleep(now);
    return;
  }

  if (PreviousButtonPressed)
  {
    if (now - lastPrevPressTime >= BUTTON_COOLDOWN)
    {
      game.OnRightKey();
      lastPrevPressTime = now;
      noteInteraction(now);
    }
    PreviousButtonPressed = false;
  }

  if (NextButtonPressed)
  {
    if (now - lastNextPressTime >= BUTTON_COOLDOWN)
    {
      game.OnLeftKey();
      lastNextPressTime = now;
      noteInteraction(now);
    }
    NextButtonPressed = false;
  }

  if (ConfirmButtonPressed)
  {
    if (now - lastConfirmPressTime >= BUTTON_COOLDOWN)
    {
      game.OnConfirmKey();
      lastConfirmPressTime = now;
      noteInteraction(now);
    }
    ConfirmButtonPressed = false;
  }
}

typedef void (*LongPressCallback)();

void handleLongPress(
    int pin,
    unsigned long thresholdMs,
    LongPressCallback callback)
{
  static unsigned long pressStartTime[20] = {0};
  static bool fired[20] = {false};

  int idx = pin % 20;

  if (digitalRead(pin) == LOW)
  {
    if (pressStartTime[idx] == 0)
      pressStartTime[idx] = millis();

    if (!fired[idx] && (millis() - pressStartTime[idx] > thresholdMs))
    {
      callback();
      fired[idx] = true;
    }
  }
  else
  {
    pressStartTime[idx] = 0;
    fired[idx] = false;
  }
}

void handleComboLongPress(
    int pinA,
    int pinB,
    unsigned long thresholdMs,
    LongPressCallback callback)
{
  static unsigned long comboStart = 0;
  static bool fired = false;

  bool bothPressed = (digitalRead(pinA) == LOW) && (digitalRead(pinB) == LOW);

  if (bothPressed)
  {
    PreviousButtonPressed = false;
    NextButtonPressed = false;
    if (comboStart == 0)
      comboStart = millis();

    if (!fired && (millis() - comboStart > thresholdMs))
    {
      callback();
      fired = true;
    }
  }
  else
  {
    comboStart = 0;
    fired = false;
  }
}

static void TFT_Reset200ms()
{
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);
  delay(10);
  digitalWrite(TFT_RST, LOW);
  delay(200);
  digitalWrite(TFT_RST, HIGH);
  delay(200);
}

void onConfirmLongPress()
{
  noteInteraction();
  digitalWrite(TFT_BLK, LOW);
  TFT_Reset200ms();
  tft.initR(TFT_INIT_TAB);
  tft.setRotation(2);
  digitalWrite(TFT_BLK, HIGH);
  game.setup_game();
}

static void onLRComboLongPress()
{
  noteInteraction();
  digitalWrite(TFT_BLK, LOW);
  game.resetPet();
  delay(1000);
  digitalWrite(TFT_BLK, HIGH);
}

static void enterLowBatteryMode(unsigned long now)
{
  if (g_lowBatteryMode)
    return;

  g_lowBatteryMode = true;
  g_lowBatteryEnteredAt = now;
  clearButtonFlags();
  isSleeping = false;
  digitalWrite(TFT_BLK, HIGH);
  game.saveNow();
  game.startBatteryAnimation();
}

static void leaveLowBatteryMode(unsigned long now)
{
  g_lowBatteryMode = false;
  g_lowBatteryEnteredAt = 0;
  g_lowBatteryRecoveredSince = 0;
  clearButtonFlags();
  if (isSleeping)
  {
    wakeFromSleep(now);
    return;
  }

  digitalWrite(TFT_BLK, HIGH);
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

    if (!g_lowBatteryMode && now - g_lowBatterySince >= LOW_BAT_CONFIRM_MS)
      enterLowBatteryMode(now);

    if (g_lowBatteryMode && !isSleeping)
    {
      game.updateBatteryAnimation(now);
      if (now - g_lowBatteryEnteredAt >= LOW_BAT_ANIMATION_MS)
        enterSleep();
    }
    return;
  }

  g_lowBatterySince = 0;
  if (!g_lowBatteryMode)
    return;

  if (g_lowBatteryRecoveredSince == 0)
    g_lowBatteryRecoveredSince = now;

  if (now - g_lowBatteryRecoveredSince >= LOW_BAT_RECOVER_CONFIRM_MS)
    leaveLowBatteryMode(now);
}

void setup()
{
  delay(1000);
  Serial.begin(115200);

  randomSeed(analogRead(0));
  Serial.println("Init Done");

  SPI.begin();   // SPI1
  SPI_2.begin(); // SPI2

  // 初始化 TFT 螢幕
  pinMode(TFT_BLK, OUTPUT);
  TFT_Reset200ms();
  tft.initR(TFT_INIT_TAB);
  digitalWrite(TFT_BLK, LOW);
  tft.setRotation(2);
  tft.setTextColor(ST77XX_RED);

  // 初始化 low battery 偵測
  initLowBatteryDetector();

  pinMode(PREVIOUS_COMMAND_BUTTON, INPUT_PULLUP);
  pinMode(CONFIRM_COMMAND_BUTTON, INPUT_PULLUP);
  pinMode(NEXT_COMMAND_BUTTON, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(CONFIRM_COMMAND_BUTTON), handleConfirmButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(NEXT_COMMAND_BUTTON), handleNextButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(PREVIOUS_COMMAND_BUTTON), handlePreviousButtonInterrupt, FALLING);

  if (!SD.begin(SD_CS, SD_SCK_MHZ(SD_SPI_MHZ)))
  {
    showSdInitError();
    return;
  }

  g_sdReady = true;
  game.setRendererAssetFormatPreference(kAssetFormatPreference);
  game.setRendererAssetAnimal(kAnimalAssetName);
  game.setup_game();
  Serial.println("Init Done");
  delay(1000);
  digitalWrite(TFT_BLK, HIGH);
  noteInteraction();
}

void loop()
{
  // 每圈都把 TFT_RST(PB14) 維持在推挽輸出 HIGH
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);

  const unsigned long now = millis();

  if (!g_sdReady)
    return;

  updateLowBatteryMode(now);
  if (g_lowBatteryMode)
    return;

  handleComboLongPress(PREVIOUS_COMMAND_BUTTON, NEXT_COMMAND_BUTTON, 2000, onLRComboLongPress);
  buttonDetect();
  if (isSleeping)
    return;

  handleLongPress(CONFIRM_COMMAND_BUTTON, 2000, onConfirmLongPress);

  if (now - lastInteractionMs >= SLEEP_TIMEOUT_MS)
  {
    enterSleep();
    return;
  }

  game.loop_game();
}
