# Renderer 資源格式

目前 `project_12` 只使用 Web exporter 產生的 `.data` asset packs。SD 卡根目錄的 `shared.data` 與 `species_<slot>.data` 存放動畫與影格；`/runtime.bin` 提供動畫引用與其他 runtime 設定。韌體不讀取舊版 `/index/` TXT manifest 或獨立 `.rle`／BMP 影格檔。舊格式範例見 [歷史 `/index/` 文件](index_txt_format.md)。

`BundleReader` 依 species slot 開啟 pack，查找動畫及影格 descriptor，將檔案定位到 payload；`FrameDecoder` 再以固定緩衝區串流解碼並分批寫入 TFT，不建立整張畫面的 framebuffer。目前接受 128×96 或 32×32 影格，以及 pack descriptor 指定的兩種 codec：

- `PAL8_RUN_LITERAL`：payload 先存 1–256 色的 RGB565 palette，再存 palette index 封包。
- `RGB565_RUN_LITERAL`：payload 直接存 RGB565 像素封包。

兩種 codec 的封包控制位元組都以最高位區分 Run／Literal，低 7 位表示像素數減一（每包 1–128 像素）。Run 後接一個像素值或 palette index，Literal 後接對應數量的值。解碼結果必須恰好等於 descriptor 的像素數與 decoded size，payload 不得有未消耗的資料；失敗會記錄資源錯誤。

資源格式與產生流程應以 Web exporter 的現行契約為準，不要按舊版單檔 RLE 說明手寫 SD 素材。
