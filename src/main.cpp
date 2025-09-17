#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include "Game.cpp"
#include <SdFat.h>

const int SD_CS  = PB0;    // ← 經 74VHC125 緩衝到 CARD_CS

// TFT (SPI2)
const int TFT_CS  = PB12;  
const int TFT_DC  = PA8;  
const int TFT_RST = PB14;  
const int TFT_BLK = PA9;

const int NEXT_COMMAND_BUTTON    = PA10; 
const int PREVIOUS_COMMAND_BUTTON= PA11; 
const int CONFIRM_COMMAND_BUTTON = PA12; 

// 建議頻率
const uint32_t SD_SPI_MHZ = 12;         // 先用 12 MHz，穩定後可升到 18 MHz

// 建立 TFT 顯示物件
// Adafruit_ST7735 tft(TFT_CS, TFT_DC, PB15 /*MOSI*/, PB13 /*SCLK*/, TFT_RST);
SPIClass SPI_2(PB15,TFT_RST,PB13);
Adafruit_ST7735 tft(&SPI_2, TFT_CS, TFT_DC, TFT_RST);

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

  SPI.begin();     // SPI1
  SPI_2.begin();   // SPI2

  // 初始化 TFT 螢幕
  pinMode(TFT_BLK, OUTPUT);
  digitalWrite(TFT_BLK, HIGH); // 打開背光
  tft.initR(INITR_BLACKTAB); // 初始化 ST7735，選擇黑底對應的設定
  tft.setRotation(2);        // 設置旋轉方向，0~3 分別對應四種方向

  pinMode(PREVIOUS_COMMAND_BUTTON, INPUT);
  pinMode(CONFIRM_COMMAND_BUTTON, INPUT);
  pinMode(NEXT_COMMAND_BUTTON, INPUT);

  attachInterrupt(digitalPinToInterrupt(CONFIRM_COMMAND_BUTTON), handleConfirmButtoInterrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(NEXT_COMMAND_BUTTON), handleNextButtonInterrupt, RISING);
  attachInterrupt(digitalPinToInterrupt(PREVIOUS_COMMAND_BUTTON), handlePreviousButtonInterrupt, RISING);

  if (!SD.begin(SD_CS, SD_SCK_MHZ(SD_SPI_MHZ)))
  {
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(0, 0);
    tft.setTextWrap(true);
    tft.print("init error");
  }

  game.setup_game();
  tft.print("Init Done");
  Serial.println("Init Done");
  delay(500);
}

void loop()
{
  buttonDetect();
  game.loop_game();
}
