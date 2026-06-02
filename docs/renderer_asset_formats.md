# Renderer 資源格式說明

目前專案為了相容性與方便檢查，會從 SD 卡渲染 BMP 圖片序列。

下一輪資源流程建議使用適合串流讀取的格式：

1. RGB565 raw 影格序列
- 最符合 ST7735 類 TFT 面板。
- 執行期間不需要解析 BMP，也不需要把 24-bit 色彩轉成 16-bit。
- 是 CPU 端最快的路徑，但需要外部 metadata 提供寬度、高度與影格數。

2. RLE 壓縮 RGB565 序列
- 用少量解碼成本換取較低的 SD 頻寬需求。
- 很適合純色區塊或重複像素較多的場景。
- 適合 SD I/O 是瓶頸的情境。

3. Sprite sheet / atlas
- 將多個影格存放在同一個資源中，降低檔案系統開銷。
- 適合影格尺寸固定，而且資源可以離線預先打包的情況。
- 開發期間仍可使用 BMP，但不應作為長期的高 FPS 格式。

目前實作策略：
- 保留 BMP 作為 fallback。
- 支援由 metadata 驅動的 `raw` RGB565 序列。
- 支援由 metadata 驅動的 `rle` RGB565 序列。
- 允許 `main.cpp` 在 `.bmp` 與 `.rle` 同時存在時，選擇要優先使用哪一種。

Renderer 使用的 RLE 檔案配置：

1. 檔案標頭
- `uint16_t width`，little-endian
- `uint16_t height`，little-endian

2. 像素串流
- 重複封包：
- `uint16_t run_length`，little-endian
- `uint16_t rgb565_color`，little-endian

3. 解碼規則
- 將每個封包展開為 `run_length` 個 `rgb565_color` 像素
- 展開後的像素總數必須等於 `width * height`
- 不使用結束標記；當所有像素剛好解碼完成時，串流即結束

注意事項：
- `bmp` 仍然是最容易人工檢查的格式。
- 當影格包含大量相同顏色區域，且 SD 頻寬是瓶頸時，`rle` 是較好的執行期間選擇。
- 當資源大小可以接受時，`raw` 仍是最簡單的高吞吐格式。
