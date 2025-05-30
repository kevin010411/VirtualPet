#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "Game.cpp"
#include <SdFat.h>

const int SD_CS = PA4;   // SD 卡 CS
const int TFT_CS = PB7;  // TFT CS
const int TFT_DC = PB6;  // TFT DC
const int TFT_RST = PB5; // TFT RST
// 按鈕引腳設定
const int NEXT_COMMAND_BUTTON = PA15;
const int CONFIRM_COMMAND_BUTTON = PA12;
const int PREVIOUS_COMMAND_BUTTON = PA11;

const unsigned long SPI_SPEED = 36000000; // 設定 SPI 速度

// 建立 TFT 顯示物件
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

SdFat SD;

Game game(&tft, &SD);

volatile bool PreviousButtonPressed = false;
volatile bool ConfirmButtonPressed = false;
volatile bool NextButtonPressed = false;

void handlePreviousButtonInterrupt()
{
  if (digitalRead(PREVIOUS_COMMAND_BUTTON) == HIGH)
    return;
  PreviousButtonPressed = true;
}
void handleConfirmButtoInterrupt()
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
  // 按鈕偵測
  if (PreviousButtonPressed)
  {
    game.PrevCommand();
    game.draw_all_layout();
    Serial.println(game.NowCommand());
    PreviousButtonPressed = false;
  }
  if (NextButtonPressed)
  {
    game.NextCommand();
    game.draw_all_layout();
    Serial.println(game.NowCommand());

    NextButtonPressed = false;
  }
  if (ConfirmButtonPressed)
  {
    Serial.println(game.NowCommand());
    game.ExecuteCommand();
    ConfirmButtonPressed = false;
  }
}

void setup()
{
  // 初始化序列埠
  Serial.begin(115200);

  randomSeed(analogRead(0));
  Serial.println("Init Done");
  SPI.begin();
  SPI.setClockDivider(SPI_CLOCK_DIV2);
  SPI.beginTransaction(SPISettings(SPI_SPEED, MSBFIRST, SPI_MODE0));
  // 初始化 TFT 螢幕
  tft.initR(INITR_BLACKTAB); // 初始化 ST7735，選擇黑底對應的設定
  tft.setRotation(2);        // 設置旋轉方向，0~3 分別對應四種方向

  pinMode(PREVIOUS_COMMAND_BUTTON, INPUT);
  pinMode(CONFIRM_COMMAND_BUTTON, INPUT);
  pinMode(NEXT_COMMAND_BUTTON, INPUT);

  attachInterrupt(digitalPinToInterrupt(CONFIRM_COMMAND_BUTTON), handleConfirmButtoInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(NEXT_COMMAND_BUTTON), handleNextButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(PREVIOUS_COMMAND_BUTTON), handlePreviousButtonInterrupt, FALLING);

  if (!SD.begin(PA4, SD_SCK_MHZ(4)))
  {
    tft.print("init error");
    return;
  }
  game.setup_game();
  Serial.println("Init Done");
  delay(500);
}

void loop()
{
  buttonDetect();
  game.loop_game();
}
