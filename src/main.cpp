#include <Adafruit_ST7735.h>
#include "Game.h"
#include <SdFat.h>

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

// 建立 TFT 顯示物件
// Adafruit_ST7735 tft(TFT_CS, TFT_DC, PB15 /*MOSI*/, PB13 /*SCLK*/, TFT_RST);
SPIClass SPI_2(PB15, TFT_RST, PB13);
Adafruit_ST7735 tft(&SPI_2, TFT_CS, TFT_DC, TFT_RST);

SdFat SD;

Game game(&tft, &SD);

const unsigned long BUTTON_COOLDOWN = 500; // 0.5 秒
unsigned long lastPrevPressTime = 0;
unsigned long lastNextPressTime = 0;
unsigned long lastConfirmPressTime = 0;
volatile bool PreviousButtonPressed = false;
volatile bool ConfirmButtonPressed = false;
volatile bool NextButtonPressed = false;

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

void buttonDetect()
{
  unsigned long now = millis();

  if (PreviousButtonPressed)
  {
    if (now - lastPrevPressTime >= BUTTON_COOLDOWN)
    {
      game.OnRightKey();
      lastPrevPressTime = now;
    }
    PreviousButtonPressed = false;
  }

  if (NextButtonPressed)
  {
    if (now - lastNextPressTime >= BUTTON_COOLDOWN)
    {
      game.OnLeftKey();
      lastNextPressTime = now;
    }
    NextButtonPressed = false;
  }

  if (ConfirmButtonPressed)
  {
    if (now - lastConfirmPressTime >= BUTTON_COOLDOWN)
    {
      game.OnConfirmKey();
      lastConfirmPressTime = now;
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
  static unsigned long pressStartTime[20] = {0}; // 依 pin index 存時間
  static bool fired[20] = {false};

  int idx = pin % 20; // 簡單對應（STM32 pin number 足夠用）

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
  digitalWrite(TFT_BLK, LOW);
  TFT_Reset200ms();
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(2);
  digitalWrite(TFT_BLK, HIGH);
  game.setup_game();
}

static void onLRComboLongPress()
{
  // digitalWrite(TFT_BLK, LOW);
  game.resetPet();
  // digitalWrite(TFT_BLK, HIGH);
}

void setup()
{
  delay(1000);
  // 初始化序列埠
  Serial.begin(115200);

  randomSeed(analogRead(0));
  Serial.println("Init Done");

  SPI.begin();   // SPI1
  SPI_2.begin(); // SPI2

  // 初始化 TFT 螢幕
  pinMode(TFT_BLK, OUTPUT);
  TFT_Reset200ms();           // 200ms reset
  tft.initR(INITR_BLACKTAB);  // 初始化 ST7735，選擇黑底對應的設定
  digitalWrite(TFT_BLK, LOW); // 打開背光
  tft.setRotation(2);         // 設置旋轉方向，0~3 分別對應四種方向
  tft.setTextColor(ST77XX_RED);

  pinMode(PREVIOUS_COMMAND_BUTTON, INPUT_PULLUP);
  pinMode(CONFIRM_COMMAND_BUTTON, INPUT_PULLUP);
  pinMode(NEXT_COMMAND_BUTTON, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(CONFIRM_COMMAND_BUTTON), handleConfirmButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(NEXT_COMMAND_BUTTON), handleNextButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(PREVIOUS_COMMAND_BUTTON), handlePreviousButtonInterrupt, FALLING);
  if (!SD.begin(SD_CS, SD_SCK_MHZ(SD_SPI_MHZ)))
  {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    tft.setTextWrap(true);
    tft.print("init error");
  }

  game.setup_game();
  // tft.print("Init Done");
  Serial.println("Init Done");
  delay(1000);
  digitalWrite(TFT_BLK, HIGH); // 打開背光
}

void loop()
{
  // 每圈都把 TFT_RST(PB14) 維持在推挽輸出 HIGH
  pinMode(TFT_RST, OUTPUT);
  digitalWrite(TFT_RST, HIGH);

  buttonDetect();
  game.loop_game();

  handleLongPress(CONFIRM_COMMAND_BUTTON, 2000, onConfirmLongPress);
  handleComboLongPress(PREVIOUS_COMMAND_BUTTON, NEXT_COMMAND_BUTTON, 2000, onLRComboLongPress);
}