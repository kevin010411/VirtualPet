# SD 資源與渲染

正式 SD bundle 由 Web exporter 產生：`runtime.bin` v19、`runtime.manifest`、
`shared.data`、`species_<slot>.data`，使用音訊時另有 `audio.data`。
舊 `/index/` TXT、獨立 RLE／BMP 檔不受支援，請重新匯出整包，不手寫資源表。

`BundleReader` 查找動畫／影格 descriptor 並定位 payload；`FrameDecoder`
以固定讀取／行緩衝解碼、分批寫 TFT，不配置整張 framebuffer。
現行 `.data` v2 codecs 為 `PAL8_RUN_LITERAL` 與 `RGB565_RUN_LITERAL`。
詳細記錄格式、尺寸及產品驗證由 Web exporter／host inspector 管理。

版面圖片由 Web 預合成，韌體依 SCREEN_BLOCKS／SCREEN_RULES 選取並顯示。
資源留在 SD 不表示相關 renderer／decoder 沒有 Flash 或 RAM 成本。
實際開檔、讀取、解碼或繪圖失敗記錄第一個資源名稱；不增加 decode-only 掃描或全檔 CRC。
