#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "Game.cpp"

const int SD_CS = PB4;   // SD 卡 CS
const int TFT_CS = PB7;  // TFT CS
const int TFT_DC = PB6;  // TFT DC
const int TFT_RST = PB5; // TFT RST
// 按鈕引腳設定
const int NEXT_COMMAND_BUTTON = PB4;
const int CONFIRM_COMMAND_BUTTON = PB5;
const int PREVIOUS_COMMAND_BUTTON = PB6;

const unsigned long SPI_SPEED = 16000000; // 設定 SPI 速度

// 建立 TFT 顯示物件
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

Game game(&tft);

volatile bool PreviousButtonPressed = false;
volatile bool ConfirmButtonPressed = false;
volatile bool NextButtonPressed = false;

void handlePreviousButtonInterrupt()
{
  PreviousButtonPressed = true;
}
void handleConfirmButtoInterrupt()
{
  ConfirmButtonPressed = true;
}
void handleNextButtonInterrupt()
{
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
  // 初始化 TFT 螢幕
  tft.initR(INITR_BLACKTAB); // 初始化 ST7735，選擇黑底對應的設定
  tft.setRotation(2);        // 設置旋轉方向，0~3 分別對應四種方向

  pinMode(PREVIOUS_COMMAND_BUTTON, INPUT_PULLUP);
  pinMode(CONFIRM_COMMAND_BUTTON, INPUT_PULLUP);
  pinMode(NEXT_COMMAND_BUTTON, INPUT_PULLUP);

  attachInterrupt(digitalPinToInterrupt(CONFIRM_COMMAND_BUTTON), handleConfirmButtoInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(NEXT_COMMAND_BUTTON), handleNextButtonInterrupt, FALLING);
  attachInterrupt(digitalPinToInterrupt(PREVIOUS_COMMAND_BUTTON), handlePreviousButtonInterrupt, FALLING);

  game.setup_game();
  Serial.println("Init Done");
  delay(500);
}

void loop()
{
  buttonDetect();
  game.loop_game();
}
