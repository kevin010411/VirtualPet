# SD 卡範例資料

跨層 Pet Stat Slot 規格以 Web repository 的
`E:\C++\virtualPet\web\.scratch\sd-driven-pet-stats\spec.md` 為唯一權威來源。
請使用 Web exporter 產生 `runtime.bin` 與 `.data` asset packs；release firmware
不接受 runtime TXT、Pet Stat alias 或 `state_schema.txt` fallback。

將 exporter 產生的完整 SD bundle 複製到 SD 卡根目錄。

Release firmware 將 `runtime.bin` 視為可信的 exporter 輸出：它只保護讀取範圍、
編譯容量與實際使用的引用，不重跑跨紀錄與產品語意驗證。請在寫入 SD 卡前使用
Web exporter 的 read-back 與 host inspector 診斷完整性問題。

- `runtime.bin`：Pet Stats、Actions、Status、Appearance、Evolution、Flow 與 Layout。
- `species_<slot>.data`、`shared.data`：動畫 asset packs。
舊版 `index/`、BMP 與獨立 RLE 影格不是目前 release firmware 的輸入；若範例目錄仍保留這些檔案，僅供歷史對照。請複製 Web exporter 產生的完整 bundle。
