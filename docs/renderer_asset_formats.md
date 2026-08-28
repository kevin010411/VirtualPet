# Renderer 資源格式說明

目前韌體從 SD 卡渲染 Literal/Run 16-bit RGB565 圖片序列。資源路徑沿用 `.rle` 副檔名與 manifest 的 `rle` 格式名稱。BMP 可保留在資源工具流程中作為開發檢查或中間格式，但不再編入韌體 runtime。

資源流程建議使用適合串流讀取的格式：

1. Literal/Run 16-bit RGB565 序列
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

Renderer 使用的 Literal/Run 16-bit 檔案配置：

1. 檔案標頭
- `uint16_t width`，little-endian
- `uint16_t height`，little-endian

2. 像素串流
- 每個封包先寫入一個 little-endian `uint16_t packet_header`
- `packet_header` 的最高位為封包種類：`0` 是 Literal、`1` 是 Run
- 低 15 位是像素數，合法範圍為 `1..32767`；`0` 無效
- Literal 封包後接 `count` 個 little-endian `uint16_t rgb565_color`
- Run 封包後只接一個 little-endian `uint16_t rgb565_color`

3. 解碼規則
- Literal 封包依序複製後續的 `count` 個顏色
- Run 封包將後續的一個顏色重複 `count` 次
- 展開後的像素總數必須等於 `width * height`
- 不使用結束標記；當所有像素剛好解碼完成時，串流即結束

注意事項：
- `bmp` 仍然是最容易人工檢查的格式。
- 當影格包含大量相同顏色區域，且 SD 頻寬是瓶頸時，`rle` 是較好的執行期間選擇。
