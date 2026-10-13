# Flash 量測與可選能力

以下是 2026-10-13 同設定 A/B 的歷史證據，不是當前建置保證。
原始案例與量測程式保留於 `.pio/flash-audit-20261013/`；
音訊證據另在 `.pio/sequence-audio-firmware-evidence/`。

| 量測 | Flash | 靜態 RAM | 解讀 |
| --- | ---: | ---: | --- |
| 外觀／圖鑑屏蔽修正前 | 62,440 B | 7,448 B | 死亡已關閉 |
| 修正後 | 61,312 B | 7,440 B | 省 1,128 B／8 B |
| 修正後再裁多段 Action 的隔離原型 | 61,152 B | 7,288 B | 非正式功能旗標 |
| 修正後再裁數值版面的隔離原型 | 60,732 B | 7,432 B | 非正式功能旗標 |
| 同時裁兩項的隔離原型 | 60,432 B | 7,272 B | LTO 差額不可相加 |

死亡開／關另一組同來源比較省 692 B Flash／16 B 靜態 RAM，
因保留已死亡存檔的基本重開路徑，不能當成刪除整套死亡功能。
音訊另一組診斷連結差額為 +3,052 B Flash／+3,004 B 靜態 RAM；
當時標準 C8 開音訊超出 944 B。128 KiB linker allowance 僅供量測，不是硬體容量證明。

## 尚未實作的裁切提案

- `ENABLE_ACTION_PLAYBACK_SEQUENCE`：依實際匯出的非空 `following_animations`
  推導，裁後續段儲存／decoder／播放解析，保留單段及共用 queue。
- `ENABLE_NUMERIC_SCREEN_LAYOUT`：依已套用版面的 stat blocks 推導，
  裁數值專用儲存、decoder、值解析與绘圖，保留一般版面及共用 Pet Stats。

兩項都只是隔離評估，正式碼沒有這些 flag。若要實作，需同步 resolver、
有界載入與能力不符处理，再用真實匯出 fixture 驗證開／關。
尺寸方法見[開發流程](development-workflow.md)，功能預設見[Profile flags](profile_feature_flags.md)。
