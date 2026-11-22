# SD 資源與渲染

## SD 卡檔案

以下是 Web 匯出的 SD ZIP 解壓到卡片根目錄後的路徑。

| 路徑 | 用途 |
| --- | --- |
| `/runtime.bin` | 執行契約。韌體從標頭讀取版本及 bundle ID。 |
| `/asset_manifest.txt` | Web 匯出與檢查工具使用的素材包清單。 |
| `/assets/shared.data` | 跨物種共用的畫面與動畫。 |
| `/assets/species_<slot>.data` | 個別物種的造型、畫面與動畫。 |
| `/audio.data` | 匯出的序列音訊。 |
| `/shop.data` | 啟用商店時匯出的商店資料。 |
| `/capture.data` | 啟用抓取遊戲時匯出的資料。 |

從舊 `/index/` TXT、獨立 RLE 或 BMP 格式升級時，透過 Web 重新匯出完整 SD ZIP。

新版 `shop.data` 的 `SBD2` 尾標記啟用自訂列表欄數、反白影格及全螢幕結算影格。韌體也保留 `SBD1` 分帶素材及更早的商店繪製路徑。商店更新應使用同一份匯出的 `shop.data`、`assets/shared.data` 和執行契約。

## 格式版本

Web 匯出器按專案使用的功能選擇 `/runtime.bin` v28 至 v32。
選版條件與圖鑑解鎖順序見[跨端規格](README.md#跨端規格)。
韌體必須支援該版本並啟用專案使用的功能。

影像素材包使用 `.data` v2。含全螢幕動畫的匯出使用 `.data` v3。
兩版都支援 `PAL8_RUN_LITERAL` 與 `RGB565_RUN_LITERAL`。

## 影格讀取

Web 預先合成版面圖片。韌體依 `SCREEN_BLOCKS` 和 `SCREEN_RULES` 選取要顯示的影格。
`BundleReader` 定位影格資料，`FrameDecoder` 以固定大小的讀取緩衝和行緩衝解碼，再分批寫入 TFT。
緩衝和解碼器分別計入韌體的 RAM 與 Flash 用量。
Web 匯出端檢查素材尺寸、格式、資料參照與完整性。韌體處理實際 SD 讀取、解碼和繪圖失敗，並記錄第一個資源錯誤。
