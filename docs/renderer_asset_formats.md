# Renderer 資源格式說明

目前韌體從 SD 卡渲染 RLE 圖片序列。BMP 可保留在資源工具流程中作為開發檢查或中間格式，但不再編入韌體 runtime。

資源流程建議使用適合串流讀取的格式：

1. RLE 壓縮 RGB565 序列
- 用少量解碼成本換取較低的 SD 頻寬需求。
- 很適合純色區塊或重複像素較多的場景。
- 適合 SD I/O 是瓶頸的情境。

2. BMP 圖片序列
- 最容易人工檢查與替換。
- 僅適合開發期間或資源檢查，不是韌體 runtime 格式。

3. Sprite sheet / atlas
- 將多個影格存放在同一個資源中，降低檔案系統開銷。
- 適合影格尺寸固定，而且資源可以離線預先打包的情況。
- 開發期間仍可使用 BMP，但不應作為長期的高 FPS 格式。

目前實作策略：
- 韌體 runtime 固定只編入 `RLERenderer`。
- `index.txt` 的格式欄位必須是 `rle`；`bmp` 行會被忽略。
- manifest 路徑超過韌體 path buffer 時，TFT 會顯示 `path error`。

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
